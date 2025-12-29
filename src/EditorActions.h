#pragma once

#include "ActionRegistry.h"
#include "Editor.h"
#include <SDL2/SDL.h>

class EditorActions {
public:
    EditorActions(ActionRegistry& registry, InputMapper& mapper)
        : registry_(registry), mapper_(mapper) {}

    void register_all(std::function<Editor*()> get_editor, std::function<int()> get_visible_lines) {
        get_editor_ = std::move(get_editor);
        get_visible_lines_ = std::move(get_visible_lines);

        register_navigation_actions();
        register_selection_actions();
        register_editing_actions();
        register_clipboard_actions();
        register_folding_actions();

        setup_default_bindings();
    }

private:
    void register_navigation_actions() {
        registry_.register_action(Actions::Editor::MoveLeft, [this]() -> ActionResult {
            if (auto* ed = get_editor_()) {
                ed->clear_selection();
                ed->reset_selection_stack();
                ed->move_left();
                return {true, true};
            }
            return {};
        });

        registry_.register_action(Actions::Editor::MoveRight, [this]() -> ActionResult {
            if (auto* ed = get_editor_()) {
                ed->clear_selection();
                ed->reset_selection_stack();
                ed->move_right();
                return {true, true};
            }
            return {};
        });

        registry_.register_action(Actions::Editor::MoveUp, [this]() -> ActionResult {
            if (auto* ed = get_editor_()) {
                ed->clear_selection();
                ed->reset_selection_stack();
                ed->move_up();
                return {true, true};
            }
            return {};
        });

        registry_.register_action(Actions::Editor::MoveDown, [this]() -> ActionResult {
            if (auto* ed = get_editor_()) {
                ed->clear_selection();
                ed->reset_selection_stack();
                ed->move_down();
                return {true, true};
            }
            return {};
        });

        registry_.register_action(Actions::Editor::MoveWordLeft, [this]() -> ActionResult {
            if (auto* ed = get_editor_()) {
                ed->clear_selection();
                ed->reset_selection_stack();
                ed->move_word_left();
                return {true, true};
            }
            return {};
        });

        registry_.register_action(Actions::Editor::MoveWordRight, [this]() -> ActionResult {
            if (auto* ed = get_editor_()) {
                ed->clear_selection();
                ed->reset_selection_stack();
                ed->move_word_right();
                return {true, true};
            }
            return {};
        });

        registry_.register_action(Actions::Editor::MoveHome, [this]() -> ActionResult {
            if (auto* ed = get_editor_()) {
                ed->clear_selection();
                ed->reset_selection_stack();
                ed->move_home();
                return {true, true};
            }
            return {};
        });

        registry_.register_action(Actions::Editor::MoveEnd, [this]() -> ActionResult {
            if (auto* ed = get_editor_()) {
                ed->clear_selection();
                ed->reset_selection_stack();
                ed->move_end();
                return {true, true};
            }
            return {};
        });

        registry_.register_action(Actions::Editor::MovePageUp, [this]() -> ActionResult {
            if (auto* ed = get_editor_()) {
                ed->clear_selection();
                ed->move_page_up(get_visible_lines_());
                return {true, true};
            }
            return {};
        });

        registry_.register_action(Actions::Editor::MovePageDown, [this]() -> ActionResult {
            if (auto* ed = get_editor_()) {
                ed->clear_selection();
                ed->move_page_down(get_visible_lines_());
                return {true, true};
            }
            return {};
        });

        registry_.register_action(Actions::Editor::MoveLineUp, [this]() -> ActionResult {
            if (auto* ed = get_editor_()) {
                ed->move_line_up();
                return {true, true};
            }
            return {};
        });

        registry_.register_action(Actions::Editor::MoveLineDown, [this]() -> ActionResult {
            if (auto* ed = get_editor_()) {
                ed->move_line_down();
                return {true, true};
            }
            return {};
        });

        registry_.register_action(Actions::Editor::GoToDefinition, [this]() -> ActionResult {
            if (auto* ed = get_editor_()) {
                bool moved = ed->go_to_definition();
                return {true, moved};
            }
            return {};
        });
    }

    void register_selection_actions() {
        registry_.register_action(Actions::Editor::SelectLeft, [this]() -> ActionResult {
            if (auto* ed = get_editor_()) {
                ed->start_selection();
                ed->reset_selection_stack();
                ed->move_left();
                return {true, true};
            }
            return {};
        });

        registry_.register_action(Actions::Editor::SelectRight, [this]() -> ActionResult {
            if (auto* ed = get_editor_()) {
                ed->start_selection();
                ed->reset_selection_stack();
                ed->move_right();
                return {true, true};
            }
            return {};
        });

        registry_.register_action(Actions::Editor::SelectUp, [this]() -> ActionResult {
            if (auto* ed = get_editor_()) {
                ed->start_selection();
                ed->reset_selection_stack();
                ed->move_up();
                return {true, true};
            }
            return {};
        });

        registry_.register_action(Actions::Editor::SelectDown, [this]() -> ActionResult {
            if (auto* ed = get_editor_()) {
                ed->start_selection();
                ed->reset_selection_stack();
                ed->move_down();
                return {true, true};
            }
            return {};
        });

        registry_.register_action(Actions::Editor::SelectWordLeft, [this]() -> ActionResult {
            if (auto* ed = get_editor_()) {
                ed->start_selection();
                ed->reset_selection_stack();
                ed->move_word_left();
                return {true, true};
            }
            return {};
        });

        registry_.register_action(Actions::Editor::SelectWordRight, [this]() -> ActionResult {
            if (auto* ed = get_editor_()) {
                ed->start_selection();
                ed->reset_selection_stack();
                ed->move_word_right();
                return {true, true};
            }
            return {};
        });

        registry_.register_action(Actions::Editor::SelectHome, [this]() -> ActionResult {
            if (auto* ed = get_editor_()) {
                ed->start_selection();
                ed->reset_selection_stack();
                ed->move_home();
                return {true, true};
            }
            return {};
        });

        registry_.register_action(Actions::Editor::SelectEnd, [this]() -> ActionResult {
            if (auto* ed = get_editor_()) {
                ed->start_selection();
                ed->reset_selection_stack();
                ed->move_end();
                return {true, true};
            }
            return {};
        });

        registry_.register_action(Actions::Editor::SelectPageUp, [this]() -> ActionResult {
            if (auto* ed = get_editor_()) {
                ed->start_selection();
                ed->move_page_up(get_visible_lines_());
                return {true, true};
            }
            return {};
        });

        registry_.register_action(Actions::Editor::SelectPageDown, [this]() -> ActionResult {
            if (auto* ed = get_editor_()) {
                ed->start_selection();
                ed->move_page_down(get_visible_lines_());
                return {true, true};
            }
            return {};
        });

        registry_.register_action(Actions::Editor::SelectAll, [this]() -> ActionResult {
            if (auto* ed = get_editor_()) {
                ed->controller.select_all(ed->document);
                return {true, true};
            }
            return {};
        });

        registry_.register_action(Actions::Editor::ExpandSelection, [this]() -> ActionResult {
            if (auto* ed = get_editor_()) {
                ed->expand_selection();
                return {true, true};
            }
            return {};
        });

        registry_.register_action(Actions::Editor::ShrinkSelection, [this]() -> ActionResult {
            if (auto* ed = get_editor_()) {
                ed->shrink_selection();
                return {true, true};
            }
            return {};
        });
    }

    void register_editing_actions() {
        registry_.register_action(Actions::Editor::NewLine, [this]() -> ActionResult {
            if (auto* ed = get_editor_()) {
                ed->clear_selection();
                ed->reset_selection_stack();
                ed->new_line();
                return {true, true};
            }
            return {};
        });

        registry_.register_action(Actions::Editor::Backspace, [this]() -> ActionResult {
            if (auto* ed = get_editor_()) {
                ed->reset_selection_stack();
                ed->backspace();
                return {true, true};
            }
            return {};
        });

        registry_.register_action(Actions::Editor::BackspaceWord, [this]() -> ActionResult {
            if (auto* ed = get_editor_()) {
                ed->reset_selection_stack();
                ed->delete_word_left();
                return {true, true};
            }
            return {};
        });

        registry_.register_action(Actions::Editor::Delete, [this]() -> ActionResult {
            if (auto* ed = get_editor_()) {
                ed->reset_selection_stack();
                ed->delete_char();
                return {true, true};
            }
            return {};
        });

        registry_.register_action(Actions::Editor::DeleteWord, [this]() -> ActionResult {
            if (auto* ed = get_editor_()) {
                ed->reset_selection_stack();
                ed->delete_word_right();
                return {true, true};
            }
            return {};
        });

        registry_.register_action(Actions::Editor::DuplicateLine, [this]() -> ActionResult {
            if (auto* ed = get_editor_()) {
                ed->duplicate_line();
                return {true, true};
            }
            return {};
        });

        registry_.register_action(Actions::Editor::ToggleComment, [this]() -> ActionResult {
            if (auto* ed = get_editor_()) {
                ed->toggle_comment();
                return {true, true};
            }
            return {};
        });

        registry_.register_action(Actions::Editor::InsertTab, [this]() -> ActionResult {
            if (auto* ed = get_editor_()) {
                ed->insert_text("    ");
                return {true, true};
            }
            return {};
        });

        registry_.register_action(Actions::Editor::Undo, [this]() -> ActionResult {
            if (auto* ed = get_editor_()) {
                bool moved = ed->undo();
                return {true, moved};
            }
            return {};
        });

        registry_.register_action(Actions::Editor::Redo, [this]() -> ActionResult {
            if (auto* ed = get_editor_()) {
                bool moved = ed->redo();
                return {true, moved};
            }
            return {};
        });
    }

    void register_clipboard_actions() {
        registry_.register_action(Actions::Editor::Copy, [this]() -> ActionResult {
            if (auto* ed = get_editor_()) {
                if (ed->has_selection()) {
                    SDL_SetClipboardText(ed->get_selected_text().c_str());
                    return {true, false};
                }
            }
            return {};
        });

        registry_.register_action(Actions::Editor::Cut, [this]() -> ActionResult {
            if (auto* ed = get_editor_()) {
                if (ed->has_selection()) {
                    SDL_SetClipboardText(ed->get_selected_text().c_str());
                    ed->delete_selection();
                    return {true, true};
                }
            }
            return {};
        });

        registry_.register_action(Actions::Editor::Paste, [this]() -> ActionResult {
            if (auto* ed = get_editor_()) {
                if (SDL_HasClipboardText()) {
                    char* clipboard = SDL_GetClipboardText();
                    ed->begin_undo_group();
                    ed->insert_text(clipboard);
                    ed->end_undo_group();
                    SDL_free(clipboard);
                    return {true, true};
                }
            }
            return {};
        });
    }

    void register_folding_actions() {
        registry_.register_action(Actions::Editor::ToggleFold, [this]() -> ActionResult {
            if (auto* ed = get_editor_()) {
                ed->toggle_fold_at_cursor();
                return {true, false};
            }
            return {};
        });

        registry_.register_action(Actions::Editor::FoldAll, [this]() -> ActionResult {
            if (auto* ed = get_editor_()) {
                ed->fold_all();
                return {true, false};
            }
            return {};
        });

        registry_.register_action(Actions::Editor::UnfoldAll, [this]() -> ActionResult {
            if (auto* ed = get_editor_()) {
                ed->unfold_all();
                return {true, false};
            }
            return {};
        });
    }

    void setup_default_bindings() {
        using namespace Actions::Editor;

        mapper_.bind({SDLK_LEFT, KeyMod::None}, MoveLeft, InputContext::Editor);
        mapper_.bind({SDLK_RIGHT, KeyMod::None}, MoveRight, InputContext::Editor);
        mapper_.bind({SDLK_UP, KeyMod::None}, MoveUp, InputContext::Editor);
        mapper_.bind({SDLK_DOWN, KeyMod::None}, MoveDown, InputContext::Editor);
        mapper_.bind({SDLK_LEFT, KeyMod::Alt}, MoveWordLeft, InputContext::Editor);
        mapper_.bind({SDLK_RIGHT, KeyMod::Alt}, MoveWordRight, InputContext::Editor);
        mapper_.bind({SDLK_HOME, KeyMod::None}, MoveHome, InputContext::Editor);
        mapper_.bind({SDLK_END, KeyMod::None}, MoveEnd, InputContext::Editor);
        mapper_.bind({SDLK_PAGEUP, KeyMod::None}, MovePageUp, InputContext::Editor);
        mapper_.bind({SDLK_PAGEDOWN, KeyMod::None}, MovePageDown, InputContext::Editor);

        mapper_.bind({SDLK_LEFT, KeyMod::Shift}, SelectLeft, InputContext::Editor);
        mapper_.bind({SDLK_RIGHT, KeyMod::Shift}, SelectRight, InputContext::Editor);
        mapper_.bind({SDLK_UP, KeyMod::Shift}, SelectUp, InputContext::Editor);
        mapper_.bind({SDLK_DOWN, KeyMod::Shift}, SelectDown, InputContext::Editor);
        mapper_.bind({SDLK_LEFT, KeyMod::AltShift}, SelectWordLeft, InputContext::Editor);
        mapper_.bind({SDLK_RIGHT, KeyMod::AltShift}, SelectWordRight, InputContext::Editor);
        mapper_.bind({SDLK_HOME, KeyMod::Shift}, SelectHome, InputContext::Editor);
        mapper_.bind({SDLK_END, KeyMod::Shift}, SelectEnd, InputContext::Editor);
        mapper_.bind({SDLK_PAGEUP, KeyMod::Shift}, SelectPageUp, InputContext::Editor);
        mapper_.bind({SDLK_PAGEDOWN, KeyMod::Shift}, SelectPageDown, InputContext::Editor);

        mapper_.bind({SDLK_UP, KeyMod::Alt}, MoveLineUp, InputContext::Editor);
        mapper_.bind({SDLK_DOWN, KeyMod::Alt}, MoveLineDown, InputContext::Editor);

        mapper_.bind({SDLK_RETURN, KeyMod::None}, NewLine, InputContext::Editor);
        mapper_.bind({SDLK_BACKSPACE, KeyMod::None}, Backspace, InputContext::Editor);
        mapper_.bind({SDLK_BACKSPACE, KeyMod::Alt}, BackspaceWord, InputContext::Editor);
        mapper_.bind({SDLK_DELETE, KeyMod::None}, Delete, InputContext::Editor);
        mapper_.bind({SDLK_DELETE, KeyMod::Alt}, DeleteWord, InputContext::Editor);
        mapper_.bind({SDLK_TAB, KeyMod::None}, InsertTab, InputContext::Editor);

        mapper_.bind({SDLK_a, KeyMod::Primary}, SelectAll, InputContext::Editor);
        mapper_.bind({SDLK_c, KeyMod::Primary}, Copy, InputContext::Editor);
        mapper_.bind({SDLK_x, KeyMod::Primary}, Cut, InputContext::Editor);
        mapper_.bind({SDLK_v, KeyMod::Primary}, Paste, InputContext::Editor);
        mapper_.bind({SDLK_z, KeyMod::Primary}, Undo, InputContext::Editor);
        mapper_.bind({SDLK_z, KeyMod::PrimaryShift}, Redo, InputContext::Editor);
        mapper_.bind({SDLK_y, KeyMod::Primary}, Redo, InputContext::Editor);

        mapper_.bind({SDLK_d, KeyMod::Primary}, DuplicateLine, InputContext::Editor);
        mapper_.bind({SDLK_SLASH, KeyMod::Primary}, ToggleComment, InputContext::Editor);

        mapper_.bind({SDLK_w, KeyMod::Primary}, ExpandSelection, InputContext::Editor);
        mapper_.bind({SDLK_w, KeyMod::PrimaryShift}, ShrinkSelection, InputContext::Editor);

        mapper_.bind({SDLK_F12, KeyMod::None}, GoToDefinition, InputContext::Editor);
        mapper_.bind({SDLK_LEFTBRACKET, KeyMod::PrimaryShift}, ToggleFold, InputContext::Editor);
        mapper_.bind({SDLK_k, KeyMod::PrimaryShift}, FoldAll, InputContext::Editor);
        mapper_.bind({SDLK_RIGHTBRACKET, KeyMod::PrimaryShift}, UnfoldAll, InputContext::Editor);
    }

    ActionRegistry& registry_;
    InputMapper& mapper_;
    std::function<Editor*()> get_editor_;
    std::function<int()> get_visible_lines_;
};
