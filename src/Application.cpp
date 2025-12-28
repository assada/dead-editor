#include "Application.h"
#include "Utils.h"
#include "LanguageRegistry.h"
#include "KeybindingsLoader.h"
#include <algorithm>
#include <filesystem>
#include <fstream>

constexpr const char* APP_NAME = "DeadEditor";

Application::Application(int argc, char* argv[]) {
    init_systems();
    init_ui();

    if (argc > 1) {
        if (is_directory(argv[1])) {
            action_open_folder(argv[1]);
        } else {
            FileLocation loc = parse_file_argument(argv[1]);
            if (action_open_file(loc.path) && loc.pos.line > 0) {
                if (auto* ed = tab_bar.get_active_editor()) {
                    ed->go_to(loc.pos);
                }
            }
        }
    }
}

Application::~Application() {
    SDL_StopTextInput();
    texture_cache.invalidate_all();
    terminal.destroy();
    font_manager.close();
    cursor_arrow.reset();
    cursor_resize_ns.reset();
    cursor_resize_ew.reset();
    renderer.reset();
    window.reset();
    TTF_Quit();
    SDL_Quit();
}

void Application::init_systems() {
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();
    register_all_languages();

    window.reset(SDL_CreateWindow(
        APP_NAME,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI
    ));

    std::string icon_path = get_resource_path("icon.bmp");
    if (SurfacePtr icon{SDL_LoadBMP(icon_path.c_str())}; icon) {
        SDL_SetWindowIcon(window.get(), icon.get());
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "best");
    renderer.reset(SDL_CreateRenderer(window.get(), -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC));

    SDL_GetRendererOutputSize(renderer.get(), &window_w, &window_h);
    layout.update(static_cast<float>(window_w) / WINDOW_WIDTH);

    cursor_arrow.reset(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW));
    cursor_resize_ns.reset(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENS));
    cursor_resize_ew.reset(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEWE));
}

void Application::init_ui() {
    auto font_result = font_manager.init(layout.scaled(DEFAULT_FONT_SIZE));
    if (!font_result) {
        SDL_Log("Failed to load font: %s", font_result.error().c_str());
        exit(EXIT_FAILURE);
    }

    font_manager.set_on_font_changed([this]() { on_font_changed(); });

    tab_bar.set_layout(&layout);
    tab_bar.set_font(font_manager.get());

    menu_bar.set_layout(&layout);
    menu_bar.set_font(font_manager.get());

    context_menu.set_layout(&layout);
    context_menu.set_font(font_manager.get());

    texture_cache.init(renderer.get(), font_manager.get());
    terminal_height = layout.scaled(250);
    tree_width = layout.file_tree_width;

    command_bar.set_layout(&layout);

    setup_actions();

    menu_bar.set_context({
        .save_file = [this]() { action_save_current(); },
        .save_file_as = [this](const std::string& path) {
            if (auto* ed = tab_bar.get_active_editor()) {
                ed->set_file_path(path);
                action_save_current();
            }
        },
        .open_file = [this](const std::string& path) { return action_open_file(path); },
        .open_folder = [this](const std::string& path) { action_open_folder(path); },
        .exit_app = [this]() { running = false; },
        .open_virtual_file = [this](const std::string& title, const std::string& content) {
            int idx = tab_bar.open_virtual_file(title, content, font_manager.get_line_height());
            if (idx >= 0) {
                tab_bar.ensure_tab_visible(window_w - get_tree_width());
                focus = FocusPanel::Editor;
                texture_cache.invalidate_all();
            }
        },
        .git_commit = [this]() {
            if (file_tree.is_git_repo()) command_bar.start_git_commit();
        },
        .git_pull = [this]() {
            if (file_tree.is_git_repo()) {
                git_pull(file_tree.root_path);
                file_tree.refresh_git_status_async();
            }
        },
        .git_push = [this]() {
            if (file_tree.is_git_repo()) git_push(file_tree.root_path);
        },
        .git_reset_hard = [this]() {
            if (file_tree.is_git_repo()) {
                git_reset_hard(file_tree.root_path);
                file_tree.refresh_git_status_async();
            }
        },
        .git_checkout = [this]() {
            if (file_tree.is_git_repo()) command_bar.start_git_checkout();
        }
    });

    SDL_StartTextInput();
}

void Application::run() {
    last_blink = SDL_GetTicks();

    while (running) {
        file_tree.apply_pending_git_status();
        file_tree.check_filesystem_changes();
        file_tree.apply_filesystem_refresh();

        process_events();
        update();
        render();
    }
}

void Application::update() {
    command_bar.clear_just_confirmed();

    if (show_terminal) {
        terminal.update();
        if (!terminal.is_running()) {
            show_terminal = false;
            focus = focus_before_terminal;
        }
    }

    Uint32 now = SDL_GetTicks();
    if (now - last_blink > CURSOR_BLINK_MS) {
        cursor_visible = !cursor_visible;
        last_blink = now;
    }

    ensure_cursor_visible();
}

void Application::process_events() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                running = false;
                break;
            case SDL_WINDOWEVENT:
                handle_window_resize(event);
                break;
            case SDL_KEYDOWN:
                dispatch_key_event(event);
                break;
            case SDL_TEXTINPUT:
                dispatch_text_input(event);
                break;
            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP:
            case SDL_MOUSEMOTION:
            case SDL_MOUSEWHEEL:
                dispatch_mouse_event(event);
                break;
        }
    }
}

void Application::handle_window_resize(const SDL_Event& event) {
    if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
        SDL_GetRendererOutputSize(renderer.get(), &window_w, &window_h);
        layout.update(static_cast<float>(window_w) / event.window.data1);
    }
}

void Application::dispatch_text_input(const SDL_Event& event) {
    reset_cursor_blink();

    if (is_meta_pressed()) return;

    if (command_bar.handle_text_input(event.text.text)) return;

    switch (focus) {
        case FocusPanel::FileTree:
            file_tree.handle_text_input(event.text.text);
            break;
        case FocusPanel::Terminal:
            if (show_terminal) terminal.write_input(event.text.text);
            break;
        case FocusPanel::Editor:
            if (auto* ed = tab_bar.get_active_editor()) {
                ed->insert_text(event.text.text);
                cursor_moved = true;
            }
            break;
    }
}

void Application::dispatch_key_event(const SDL_Event& event) {
    reset_cursor_blink();

    if (command_bar.is_active()) {
        handle_command_bar_key(event);
        return;
    }

    InputContext context = get_current_input_context();

    if (show_terminal && context == InputContext::Terminal) {
        auto result = action_registry.try_execute(event.key, input_mapper, InputContext::Terminal);
        if (result.consumed) return;
    }

    auto result = action_registry.try_execute(event.key, input_mapper, context);
    if (result.consumed) {
        if (result.cursor_moved) cursor_moved = true;
        return;
    }

    result = action_registry.try_execute(event.key, input_mapper, InputContext::Global);
    if (result.consumed) {
        if (result.cursor_moved) cursor_moved = true;
        return;
    }

    if (focus == FocusPanel::Terminal && show_terminal) {
        terminal.handle_key_event(event);
        return;
    }
}

InputContext Application::get_current_input_context() const {
    if (command_bar.is_active()) return InputContext::CommandBar;
    switch (focus) {
        case FocusPanel::Editor: return InputContext::Editor;
        case FocusPanel::FileTree: return InputContext::FileTree;
        case FocusPanel::Terminal: return InputContext::Terminal;
    }
    return InputContext::Global;
}

void Application::setup_actions() {
    editor_actions_ = std::make_unique<EditorActions>(action_registry, input_mapper);
    editor_actions_->register_all(
        [this]() -> Editor* { return tab_bar.get_active_editor(); },
        [this]() -> int { return get_content_height() / font_manager.get_line_height(); }
    );

    app_actions_ = std::make_unique<AppActions>(action_registry, input_mapper);
    app_actions_->register_all({
        .save_current = [this]() { action_save_current(); },
        .start_search = [this]() { command_bar.start_search(); },
        .start_goto = [this]() { command_bar.start_goto(); },
        .find_next = [this](const std::string& query, TextPos start) {
            if (auto* ed = tab_bar.get_active_editor()) {
                if (ed->find_next(query, start)) cursor_moved = true;
            }
        },
        .get_search_query = [this]() { return command_bar.get_search_query(); },
        .get_cursor_pos = [this]() -> TextPos {
            if (auto* ed = tab_bar.get_active_editor()) return ed->cursor_pos();
            return {};
        },
        .toggle_focus = [this]() { toggle_focus(); },
        .focus_terminal = [this]() {
            if (show_terminal) focus = FocusPanel::Terminal;
        },
        .toggle_terminal = [this]() { toggle_terminal(); },
        .next_tab = [this]() {
            tab_bar.next_tab();
            tab_bar.ensure_tab_visible(window_w - get_tree_width());
            if (auto* ed = tab_bar.get_active_editor()) {
                update_title(ed->get_file_path());
                texture_cache.invalidate_all();
            }
        },
        .prev_tab = [this]() {
            tab_bar.prev_tab();
            tab_bar.ensure_tab_visible(window_w - get_tree_width());
            if (auto* ed = tab_bar.get_active_editor()) {
                update_title(ed->get_file_path());
                texture_cache.invalidate_all();
            }
        },
        .close_active_tab = [this]() {
            int active = tab_bar.get_active_index();
            if (active >= 0) action_close_tab(active);
        },
        .zoom_in = [this]() { font_manager.increase_size(); },
        .zoom_out = [this]() { font_manager.decrease_size(); },
        .zoom_reset = [this]() { font_manager.reset_size(); },
        .terminal_resize_up = [this]() {
            terminal_height = std::min(terminal_height + layout.terminal_resize_step,
                                       std::min(layout.terminal_max, window_h - layout.status_bar_height - layout.scaled(100)));
            if (terminal.is_running()) terminal.resize(window_w - layout.padding * 2, terminal_height - layout.padding * 2);
        },
        .terminal_resize_down = [this]() {
            terminal_height = std::max(terminal_height - layout.terminal_resize_step, layout.terminal_min);
            if (terminal.is_running()) terminal.resize(window_w - layout.padding * 2, terminal_height - layout.padding * 2);
        },
        .terminal_paste = [this]() {
            if (char* clipboard = SDL_GetClipboardText()) {
                if (clipboard[0]) terminal.write_input(clipboard);
                SDL_free(clipboard);
            }
        },
        .quit = [this]() { running = false; },
        .git_commit = [this]() {
            if (file_tree.is_git_repo()) command_bar.start_git_commit();
        }
    });

    filetree_actions_ = std::make_unique<FileTreeActions>(action_registry, input_mapper);
    filetree_actions_->register_all(
        &file_tree,
        [this]() -> int { return get_content_height() / font_manager.get_line_height(); },
        [this]() -> bool { return tab_bar.has_tabs(); },
        {
            .get_selected = [this]() -> FileTreeNode* { return file_tree.get_selected(); },
            .open_file = [this](const std::string& path) {
                if (action_open_file(path)) cursor_moved = true;
            },
            .focus_editor = [this]() { focus = FocusPanel::Editor; },
            .quit = [this]() { running = false; },
            .start_create = [this](const std::string& path) { command_bar.start_create(path); },
            .start_delete = [this](const std::string& path, const std::string& name) {
                command_bar.start_delete(path, name);
            }
        }
    );

    std::string config_path = get_config_path("keybindings.json");
    KeybindingsLoader::load_from_json(input_mapper, config_path);
}

void Application::handle_command_bar_key(const SDL_Event& event) {
    auto result = command_bar.handle_key(event);

    if (result.action == CommandAction::Confirm) {
        switch (result.mode) {
            case CommandMode::Delete:
                action_delete_node(result.path);
                break;
            case CommandMode::Create:
                action_create_node(result.path, result.input);
                break;
            case CommandMode::Rename:
                action_rename_node(result.path, result.input);
                break;
            case CommandMode::GoTo:
                if (auto* ed = tab_bar.get_active_editor()) {
                    ed->go_to(result.pos);
                    cursor_moved = true;
                }
                break;
            case CommandMode::SavePrompt: {
                int pending = tab_bar.get_pending_close_tab();
                if (result.input == "save" && pending >= 0) {
                    if (Tab* tab = tab_bar.get_tab_mut(pending)) {
                        if (tab->editor) tab->editor->save_file();
                        file_tree.refresh_git_status_async();
                    }
                    tab_bar.close_tab(pending);
                } else if (result.input == "discard" && pending >= 0) {
                    tab_bar.close_tab(pending);
                }
                tab_bar.clear_pending_close();
                if (!tab_bar.has_tabs()) focus = FocusPanel::FileTree;
                break;
            }
            case CommandMode::GitCommit:
                if (!result.input.empty() && file_tree.is_git_repo()) {
                    git_commit(file_tree.root_path, result.input);
                    file_tree.refresh_git_status_async();
                }
                break;
            case CommandMode::GitCheckout:
                if (!result.input.empty() && file_tree.is_git_repo()) {
                    git_checkout(file_tree.root_path, result.input);
                    file_tree.refresh_git_status_async();
                }
                break;
            default:
                break;
        }
    } else if (result.action == CommandAction::FindNext) {
        if (auto* ed = tab_bar.get_active_editor()) {
            int next_col = ed->get_cursor_col() + static_cast<int>(result.input.size());
            if (ed->find_next(result.input, {ed->get_cursor_line(), next_col})) {
                cursor_moved = true;
            }
        }
    } else if (result.action == CommandAction::Cancel) {
        tab_bar.clear_pending_close();
    }
}

void Application::dispatch_mouse_event(const SDL_Event& event) {
    int mx = layout.mouse_x(event.button.x);
    int my = layout.mouse_y(event.button.y);
    int term_h = get_terminal_height();
    int tree_w = get_tree_width();
    int tab_h = tab_bar.has_tabs() ? layout.tab_bar_height : 0;
    int term_y = window_h - layout.status_bar_height - term_h;

    if (event.type == SDL_MOUSEWHEEL) {
        int wmx, wmy;
        SDL_GetMouseState(&wmx, &wmy);
        wmx = layout.mouse_x(wmx);
        wmy = layout.mouse_y(wmy);

        if (show_terminal && wmy >= term_y && wmy < window_h - layout.status_bar_height) {
            terminal.handle_mouse_wheel(event.wheel.y);
        } else if (tab_bar.has_tabs() && wmx >= tree_w && wmy >= layout.menu_bar_height && wmy < layout.menu_bar_height + tab_h) {
            tab_bar.handle_scroll(event.wheel.y, window_w - tree_w);
        } else if (file_tree.is_loaded() && wmx < tree_w && wmy >= layout.menu_bar_height && wmy < term_y) {
            int vis = (term_y - layout.menu_bar_height - layout.padding * 2) / font_manager.get_line_height();
            file_tree.handle_scroll(event.wheel.y, vis);
        } else if (auto* ed = tab_bar.get_active_editor()) {
            const Uint8* keys = SDL_GetKeyboardState(nullptr);
            bool shift = keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT];
            ed->handle_scroll(event.wheel.x, event.wheel.y, font_manager.get_char_width(), shift);
        }
        return;
    }

    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        if (context_menu.is_open()) {
            context_menu.handle_mouse_click(mx, my);
            file_tree.context_menu_index = -1;
            return;
        }

        if (menu_bar.handle_mouse_click(mx, my)) {
            context_menu.close();
            file_tree.context_menu_index = -1;
            menu_click_consumed = true;
            return;
        }
        if (my < layout.menu_bar_height) {
            menu_click_consumed = true;
            return;
        }

        if (show_terminal && my >= term_y - 5 && my <= term_y + 5) {
            dragging.terminal = true;
            SDL_SetCursor(cursor_resize_ns.get());
        }
        if (file_tree.is_loaded() && !dragging.terminal && mx >= tree_w - 5 && mx <= tree_w + 5 && my >= layout.menu_bar_height && my < term_y) {
            dragging.tree = true;
            SDL_SetCursor(cursor_resize_ew.get());
        }

        if (!dragging.terminal && !dragging.tree) {
            int editor_y = layout.menu_bar_height + tab_h;

            if (tab_bar.has_tabs() && mx >= tree_w && my >= layout.menu_bar_height && my < editor_y) {
                auto click = tab_bar.handle_mouse_click(mx - tree_w, my - layout.menu_bar_height, false);
                if (click.action == TabAction::SwitchTab) {
                    tab_bar.switch_to_tab(click.tab_index);
                    if (auto* ed = tab_bar.get_active_editor()) {
                        update_title(ed->get_file_path());
                        texture_cache.invalidate_all();
                        cursor_moved = true;
                    }
                    focus = FocusPanel::Editor;
                } else if (click.action == TabAction::CloseTab) {
                    action_close_tab(click.tab_index);
                } else if (click.action == TabAction::CloseModifiedTab) {
                    if (const Tab* tab = tab_bar.get_tab(click.tab_index)) {
                        tab_bar.try_close_tab(click.tab_index);
                        command_bar.start_save_prompt(tab->title);
                    }
                }
            } else if (show_terminal && my >= term_y && my < window_h - layout.status_bar_height) {
                focus = FocusPanel::Terminal;
            } else if (file_tree.is_loaded() && mx < tree_w && my >= layout.menu_bar_height && my < term_y) {
                focus = FocusPanel::FileTree;
            } else if (mx >= tree_w && my >= editor_y && my < term_y) {
                focus = FocusPanel::Editor;
            }
        }

        if (!dragging.terminal && !dragging.tree && tab_bar.has_tabs()) {
            int editor_y = layout.menu_bar_height + tab_h;
            if (auto* ed = tab_bar.get_active_editor()) {
                if (mx >= tree_w && my >= editor_y && my < term_y) {
                    if (is_meta_pressed()) {
                        ed->update_cursor_from_mouse(mx, my, tree_w, editor_y, font_manager.get());
                        if (ed->go_to_definition()) cursor_moved = true;
                    } else if (event.button.clicks == 2) {
                        ed->handle_mouse_double_click(mx, my, tree_w, editor_y, font_manager.get());
                    } else {
                        int ed_visible_w = window_w - tree_w;
                        int ed_visible_h = term_y - editor_y;
                        ed->handle_mouse_click(mx, my, tree_w, editor_y, ed_visible_w, ed_visible_h, font_manager.get());
                        if (!ed->is_scrollbar_dragging()) {
                            dragging.editor = true;
                            cursor_moved = true;
                        }
                    }
                }
            }
        }
        return;
    }

    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_RIGHT) {
        if (context_menu.is_open()) {
            context_menu.close();
            file_tree.context_menu_index = -1;
        }

        int editor_y = layout.menu_bar_height + tab_h;
        if (tab_bar.has_tabs() && mx >= tree_w && my >= layout.menu_bar_height && my < editor_y) {
            auto click = tab_bar.handle_mouse_click(mx - tree_w, my - layout.menu_bar_height, true);
            if (click.action == TabAction::ShowContextMenu) {
                int clicked_tab = click.tab_index;
                std::vector<ContextMenuItem> items;

                items.push_back({"Close", [this, clicked_tab]() {
                    action_close_tab(clicked_tab);
                }, true, false});

                bool has_others = tab_bar.get_tab_count() > 1;
                items.push_back({"Close Others", [this, clicked_tab]() {
                    auto others = tab_bar.get_other_tabs(clicked_tab);
                    tab_bar.close_tabs(others);
                    if (!tab_bar.has_tabs()) {
                        focus = FocusPanel::FileTree;
                        update_title();
                    }
                }, has_others, false});

                items.push_back({"Close All", [this]() {
                    auto all = tab_bar.get_all_tabs();
                    tab_bar.close_tabs(all);
                    if (!tab_bar.has_tabs()) {
                        focus = FocusPanel::FileTree;
                        update_title();
                    }
                }, true, true});

                auto saved = tab_bar.get_saved_tabs();
                items.push_back({"Close Saved", [this]() {
                    auto saved_tabs = tab_bar.get_saved_tabs();
                    tab_bar.close_tabs(saved_tabs);
                    if (!tab_bar.has_tabs()) {
                        focus = FocusPanel::FileTree;
                        update_title();
                    }
                }, !saved.empty(), false});

                context_menu.show(mx, my, std::move(items), window_w, window_h);
            }
            return;
        }

        if (file_tree.is_loaded() && mx < tree_w && my >= layout.menu_bar_height && my < term_y) {
            int local_y = my - layout.menu_bar_height;
            int node_index = file_tree.get_index_at_position(local_y, font_manager.get_line_height());
            FileTreeNode* node = node_index >= 0 ? file_tree.get_node_at_position(local_y, font_manager.get_line_height()) : nullptr;
            file_tree.context_menu_index = node_index;
            std::vector<ContextMenuItem> items;

            if (node) {
                std::string base_path = node->is_directory
                    ? node->full_path
                    : std::filesystem::path(node->full_path).parent_path().string();

                items.push_back({"New File...", [this, base_path]() {
                    command_bar.start_create(base_path);
                }, true, false});

                bool can_modify = node->full_path != file_tree.root_path;
                items.push_back({"Rename...", [this, path = node->full_path, name = node->name]() {
                    command_bar.start_rename(path, name);
                }, can_modify, false});

                items.push_back({"Delete", [this, path = node->full_path, name = node->name]() {
                    command_bar.start_delete(path, name);
                }, can_modify, true});

                items.push_back({"Copy Path", [path = node->full_path]() {
                    SDL_SetClipboardText(path.c_str());
                }, true, false});

                std::string relative_path = node->full_path;
                if (relative_path.find(file_tree.root_path) == 0) {
                    relative_path = relative_path.substr(file_tree.root_path.size());
                    if (!relative_path.empty() && relative_path[0] == '/') {
                        relative_path = relative_path.substr(1);
                    }
                }
                items.push_back({"Copy Relative Path", [relative_path]() {
                    SDL_SetClipboardText(relative_path.c_str());
                }, true, false});

                items.push_back({"Open Containing Folder", [path = node->full_path]() {
                    open_containing_folder(path);
                }, true, true});

                if (file_tree.is_git_repo()) {
                    bool is_staged = file_tree.is_file_staged(node->full_path);
                    bool is_untracked = file_tree.is_file_untracked(node->full_path);
                    bool is_modified = file_tree.is_file_modified(node->full_path);

                    if (is_untracked || is_modified || !is_staged) {
                        items.push_back({"Git Add", [this, path = node->full_path]() {
                            git_add(file_tree.root_path, path);
                            file_tree.refresh_git_status_async();
                        }, true, false});
                    }

                    if (is_staged) {
                        items.push_back({"Git Unstage", [this, path = node->full_path]() {
                            git_unstage(file_tree.root_path, path);
                            file_tree.refresh_git_status_async();
                        }, true, false});
                    }
                }
            } else {
                items.push_back({"New File...", [this]() {
                    command_bar.start_create(file_tree.root_path);
                }, true, false});
            }

            context_menu.show(mx, my, std::move(items), window_w, window_h);
        }
        return;
    }

    if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT) {
        if (auto* ed = tab_bar.get_active_editor()) {
            ed->handle_mouse_up();
        }
        if (dragging.terminal) { dragging.terminal = false; SDL_SetCursor(cursor_arrow.get()); }
        if (dragging.tree) { dragging.tree = false; SDL_SetCursor(cursor_arrow.get()); }
        if (menu_click_consumed) { menu_click_consumed = false; dragging.editor = false; return; }

        if (file_tree.is_loaded() && mx < tree_w && my >= layout.menu_bar_height && my < term_y) {
            int local_y = my - layout.menu_bar_height;
            if (event.button.clicks == 2) {
                std::string path = file_tree.handle_mouse_double_click(mx, local_y, font_manager.get_line_height());
                if (!path.empty() && action_open_file(path)) cursor_moved = true;
            } else if (!dragging.editor) {
                file_tree.handle_mouse_click(mx, local_y, font_manager.get_line_height());
            }
        }
        dragging.editor = false;
        return;
    }

    if (event.type == SDL_MOUSEMOTION) {
        int motion_x = layout.mouse_x(event.motion.x);
        int motion_y = layout.mouse_y(event.motion.y);

        if (context_menu.is_open()) {
            context_menu.handle_mouse_motion(motion_x, motion_y);
        }

        if (dragging.terminal) {
            int new_h = window_h - layout.status_bar_height - motion_y;
            terminal_height = std::clamp(new_h, layout.terminal_min, std::min(layout.terminal_max, window_h - layout.status_bar_height - layout.scaled(100)));
            if (terminal.is_running()) terminal.resize(window_w - layout.padding * 2, terminal_height - layout.padding * 2);
            return;
        }
        if (dragging.tree) {
            tree_width = std::clamp(motion_x, layout.file_tree_min, layout.file_tree_max);
            return;
        }
        if (auto* ed = tab_bar.get_active_editor()) {
            int editor_y = layout.menu_bar_height + tab_h;
            int ed_visible_w = window_w - tree_w;
            int ed_visible_h = term_y - editor_y;

            if (ed->is_scrollbar_dragging()) {
                ed->handle_mouse_drag(motion_x, motion_y, tree_w, editor_y, ed_visible_w, ed_visible_h, font_manager.get());
                return;
            }

            ed->handle_mouse_move(motion_x, motion_y, tree_w, editor_y, ed_visible_w, ed_visible_h);

            if (dragging.editor) {
                ed->handle_mouse_drag(motion_x, motion_y, tree_w, editor_y, ed_visible_w, ed_visible_h, font_manager.get());
                cursor_moved = true;
                return;
            }
        }

        menu_bar.handle_mouse_motion(motion_x, motion_y);
        if (tab_bar.has_tabs() && motion_x >= tree_w && motion_y >= layout.menu_bar_height && motion_y < layout.menu_bar_height + tab_h) {
            tab_bar.handle_mouse_motion(motion_x - tree_w, motion_y - layout.menu_bar_height);
        }

        bool on_tree_border = file_tree.is_loaded() && motion_x >= tree_w - 5 && motion_x <= tree_w + 5 && motion_y >= layout.menu_bar_height && motion_y < term_y;
        bool on_term_border = show_terminal && motion_y >= term_y - 5 && motion_y <= term_y + 5;

        if (on_tree_border) SDL_SetCursor(cursor_resize_ew.get());
        else if (on_term_border) SDL_SetCursor(cursor_resize_ns.get());
        else SDL_SetCursor(cursor_arrow.get());
    }
}

void Application::render() {
    SDL_SetRenderDrawColor(renderer.get(), Colors::BG.r, Colors::BG.g, Colors::BG.b, 255);
    SDL_RenderClear(renderer.get());

    int line_h = font_manager.get_line_height();
    int tree_w = get_tree_width();
    int tab_h = tab_bar.has_tabs() ? layout.tab_bar_height : 0;

    int status_bar_y = window_h - layout.status_bar_height;
    int term_h = show_terminal ? terminal_height : 0;
    int terminal_y = status_bar_y - term_h;
    int cmd_h = command_bar.is_active() ? layout.search_bar_height : 0;
    int command_bar_y = terminal_y - cmd_h;
    int content_y = layout.menu_bar_height + tab_h;
    int content_h = command_bar_y - content_y;

    Editor* ed = tab_bar.get_active_editor();
    if (ed) ed->update_highlight_occurrences();

    menu_bar.render(renderer.get(), texture_cache, window_w, line_h);

    if (file_tree.is_loaded()) {
        std::string cur_path = ed ? ed->get_file_path() : "";
        file_tree.render(renderer.get(), font_manager.get(), texture_cache,
                        0, layout.menu_bar_height, tree_w, command_bar_y - layout.menu_bar_height, line_h,
                        focus == FocusPanel::FileTree, cursor_visible, cur_path);
    }

    if (tab_bar.has_tabs()) {
        tab_bar.render(renderer.get(), texture_cache, tree_w, layout.menu_bar_height,
                      window_w - tree_w, line_h, file_tree.is_loaded() ? &file_tree : nullptr);
    }

    if (ed) {
        ed->render(renderer.get(), font_manager.get(), texture_cache,
                  command_bar.get_search_query(),
                  tree_w, content_y,
                  window_w - tree_w, content_h,
                  window_w, font_manager.get_char_width(),
                  focus == FocusPanel::Editor, tab_bar.has_tabs(), cursor_visible,
                  layout,
                  [this](TokenType t) { return get_syntax_color(t); });
    }

    command_bar.render(renderer.get(), font_manager.get(), texture_cache,
                      0, command_bar_y, window_w, line_h, cursor_visible);

    EditorStatus status;
    if (ed) {
        status.file_path = ed->get_file_path();
        status.modified = ed->is_modified();
        status.cursor_pos = ed->cursor_pos();
        status.total_lines = static_cast<int>(ed->get_lines().size());
    }
    command_bar.render_status_bar(renderer.get(), texture_cache,
                                  0, status_bar_y, window_w, line_h, status, file_tree.git_branch);

    if (show_terminal && terminal.is_running()) {
        SDL_SetRenderDrawColor(renderer.get(), 18, 18, 22, 255);
        SDL_Rect term_bg = {0, terminal_y, window_w, terminal_height};
        SDL_RenderFillRect(renderer.get(), &term_bg);

        SDL_SetRenderDrawColor(renderer.get(), 60, 60, 70, 255);
        SDL_RenderDrawLine(renderer.get(), 0, terminal_y, window_w, terminal_y);

        terminal.render(renderer.get(), font_manager.get(), layout.padding, terminal_y + layout.padding,
                       window_w - layout.padding * 2, terminal_height - layout.padding * 2);
    }

    menu_bar.render_dropdown_overlay(renderer.get(), texture_cache, line_h);
    context_menu.render(renderer.get(), texture_cache, line_h);

    SDL_RenderPresent(renderer.get());
}

bool Application::action_open_file(const std::string& path) {
    int idx = tab_bar.open_file(path, font_manager.get_line_height(), true);
    if (idx >= 0) {
        tab_bar.ensure_tab_visible(window_w - get_tree_width());
        focus = FocusPanel::Editor;
        update_title(path);
        texture_cache.invalidate_all();
        return true;
    }
    return false;
}

void Application::action_open_folder(const std::string& path) {
    while (tab_bar.has_tabs()) tab_bar.close_tab(0);
    texture_cache.invalidate_all();
    file_tree.load_directory(path);
    file_tree.active = true;
    tree_width = layout.file_tree_width;
    focus = FocusPanel::FileTree;
    update_title(path);
}

void Application::action_save_current() {
    if (auto* ed = tab_bar.get_active_editor()) {
        if (ed->save_file()) {
            tab_bar.update_active_title();
            update_title(ed->get_file_path());
            file_tree.refresh_git_status_async();
        }
    }
}

void Application::action_close_tab(int index) {
    if (!tab_bar.try_close_tab(index)) {
        if (const Tab* tab = tab_bar.get_tab(index)) {
            command_bar.start_save_prompt(tab->title);
        }
    } else {
        if (tab_bar.has_tabs()) {
            if (auto* ed = tab_bar.get_active_editor()) update_title(ed->get_file_path());
        } else {
            update_title();
            focus = FocusPanel::FileTree;
        }
    }
}

void Application::action_create_node(const std::string& base_path, const std::string& name) {
    std::string full_path = base_path + "/" + name;
    bool is_dir = !name.empty() && name.back() == '/';
    if (is_dir) full_path.pop_back();

    try {
        std::filesystem::path p(full_path);
        std::string canonical = std::filesystem::weakly_canonical(p).string();

        if (is_dir) {
            std::filesystem::create_directories(p);
        } else {
            std::filesystem::create_directories(p.parent_path());
            std::ofstream(full_path).close();
        }

        file_tree.load_directory(file_tree.root_path);
        file_tree.expand_and_select_path(canonical);

        if (!is_dir) action_open_file(canonical);
    } catch (...) {}
}

void Application::action_delete_node(const std::string& path) {
    try {
        std::vector<int> tabs_to_close;
        std::string target = std::filesystem::canonical(path).string();

        for (int i = 0; i < tab_bar.get_tab_count(); i++) {
            if (const Tab* tab = tab_bar.get_tab(i)) {
                if (!tab->get_path().empty()) {
                    std::string tab_path = std::filesystem::canonical(tab->get_path()).string();
                    if (tab_path == target || tab_path.find(target + "/") == 0) {
                        tabs_to_close.push_back(i);
                    }
                }
            }
        }

        std::string parent = std::filesystem::path(path).parent_path().string();
        int old_idx = file_tree.selected_index;

        file_tree.save_expanded_state();
        std::filesystem::remove_all(path);
        file_tree.load_directory(file_tree.root_path);
        file_tree.restore_expanded_state();
        file_tree.rebuild_visible();

        if (old_idx >= static_cast<int>(file_tree.visible_nodes.size())) {
            old_idx = static_cast<int>(file_tree.visible_nodes.size()) - 1;
        }
        if (old_idx >= 0) file_tree.selected_index = old_idx;

        for (int i = static_cast<int>(tabs_to_close.size()) - 1; i >= 0; i--) {
            tab_bar.close_tab(tabs_to_close[i]);
        }
        if (!tab_bar.has_tabs()) focus = FocusPanel::FileTree;
    } catch (...) {}
}

void Application::action_rename_node(const std::string& old_path, const std::string& new_name) {
    try {
        std::filesystem::path old_fs_path(old_path);
        std::filesystem::path new_fs_path = old_fs_path.parent_path() / new_name;
        std::string new_path = new_fs_path.string();

        std::filesystem::rename(old_fs_path, new_fs_path);

        for (int i = 0; i < tab_bar.get_tab_count(); i++) {
            if (Tab* tab = tab_bar.get_tab_mut(i)) {
                if (!tab->get_path().empty()) {
                    std::string tab_path = tab->get_path();
                    if (tab_path == old_path) {
                        tab->editor->set_file_path(new_path);
                        tab->update_title();
                    } else if (tab_path.find(old_path + "/") == 0) {
                        std::string relative = tab_path.substr(old_path.size());
                        tab->editor->set_file_path(new_path + relative);
                        tab->update_title();
                    }
                }
            }
        }

        file_tree.save_expanded_state();
        file_tree.load_directory(file_tree.root_path);
        file_tree.restore_expanded_state();
        file_tree.rebuild_visible();
        file_tree.expand_and_select_path(new_path);

        texture_cache.invalidate_all();

        if (auto* ed = tab_bar.get_active_editor()) {
            update_title(ed->get_file_path());
        }
    } catch (...) {}
}

void Application::toggle_terminal() {
    show_terminal = !show_terminal;
    if (show_terminal) {
        focus_before_terminal = focus;
        if (!terminal.is_running()) {
            terminal.spawn(window_w - layout.padding * 2, terminal_height - layout.padding * 2,
                          font_manager.get_char_width(), font_manager.get_terminal_line_height(),
                          &focus, renderer.get(), font_manager.get());
        }
        focus = FocusPanel::Terminal;
    } else {
        focus = focus_before_terminal;
    }
}

void Application::toggle_focus() {
    if (show_terminal && focus == FocusPanel::Terminal) {
        focus = FocusPanel::Editor;
    } else if (file_tree.is_loaded()) {
        if (focus == FocusPanel::FileTree && tab_bar.has_tabs()) {
            focus = FocusPanel::Editor;
        } else if (focus == FocusPanel::Editor) {
            focus = FocusPanel::FileTree;
        }
    }
}

void Application::update_title(const std::string& path) {
    std::string title = APP_NAME;
    if (!path.empty()) title += " - " + path;
    SDL_SetWindowTitle(window.get(), title.c_str());
}

void Application::ensure_cursor_visible() {
    if (!cursor_moved) return;

    if (auto* ed = tab_bar.get_active_editor()) {
        int visible = get_content_height() / font_manager.get_line_height();
        ed->ensure_visible(visible);

        int tree_w = get_tree_width();
        int visible_w = window_w - tree_w - layout.gutter_width - layout.padding;
        int cursor_px = 0;
        if (ed->get_cursor_col() > 0 && !ed->get_lines()[ed->get_cursor_line()].empty()) {
            TTF_SizeUTF8(font_manager.get(), ed->get_lines()[ed->get_cursor_line()].substr(0, ed->get_cursor_col()).c_str(), &cursor_px, nullptr);
        }
        ed->ensure_visible_x(cursor_px, visible_w, font_manager.get_char_width() * 2);
    }
    cursor_moved = false;
}

SDL_Color Application::get_syntax_color(TokenType type) {
    switch (type) {
        case TokenType::Keyword: return Colors::SYNTAX_KEYWORD;
        case TokenType::Type: return Colors::SYNTAX_TYPE;
        case TokenType::String:
        case TokenType::Char: return Colors::SYNTAX_STRING;
        case TokenType::Comment: return Colors::SYNTAX_COMMENT;
        case TokenType::Number: return Colors::SYNTAX_NUMBER;
        case TokenType::Preprocessor: return Colors::SYNTAX_PREPROC;
        case TokenType::Function: return Colors::SYNTAX_FUNCTION;
        case TokenType::Variable: return Colors::SYNTAX_VARIABLE;
        default: return Colors::TEXT;
    }
}

void Application::on_font_changed() {
    texture_cache.set_font(font_manager.get());
    tab_bar.set_font(font_manager.get());
    tab_bar.invalidate_all_caches();
    menu_bar.set_font(font_manager.get());
    context_menu.set_font(font_manager.get());
    for (int i = 0; i < tab_bar.get_tab_count(); i++) {
        if (auto* tab = tab_bar.get_tab_mut(i)) {
            tab->editor->set_line_height(font_manager.get_line_height());
        }
    }
}

void Application::reset_cursor_blink() {
    cursor_visible = true;
    last_blink = SDL_GetTicks();
}

int Application::get_tree_width() const {
    return file_tree.is_loaded() ? tree_width : 0;
}

int Application::get_terminal_height() const {
    return show_terminal ? terminal_height : 0;
}

int Application::get_content_height() const {
    int status_bar_y = window_h - layout.status_bar_height;
    int term_h = show_terminal ? terminal_height : 0;
    int terminal_y = status_bar_y - term_h;
    int cmd_h = command_bar.is_active() ? layout.search_bar_height : 0;
    int command_bar_y = terminal_y - cmd_h;
    int tab_h = tab_bar.has_tabs() ? layout.tab_bar_height : 0;
    int content_y = layout.menu_bar_height + tab_h;
    return command_bar_y - content_y;
}
