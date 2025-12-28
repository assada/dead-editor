#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string>
#include <vector>
#include <functional>
#include "Constants.h"
#include "Layout.h"
#include "TextureCache.h"
#include "HelpContent.h"
#include "Utils.h"

enum class MenuAction {
    None,
    Save,
    SaveAs,
    OpenFile,
    OpenFolder,
    Exit,
    About,
    Keymap,
    GitCommit,
    GitPull,
    GitPush,
    GitResetHard,
    GitCheckout
};

struct MenuContext {
    std::function<void()> save_file;
    std::function<void(const std::string&)> save_file_as;
    std::function<void(const std::string&)> open_file;
    std::function<void(const std::string&)> open_folder;
    std::function<void()> exit_app;
    std::function<void(const std::string&, const std::string&)> open_virtual_file;
    std::function<void()> git_commit;
    std::function<void()> git_pull;
    std::function<void()> git_push;
    std::function<void()> git_reset_hard;
    std::function<void()> git_checkout;
};

struct MenuItem {
    std::string label;
    std::string shortcut;
    MenuAction action = MenuAction::None;
    bool separator_after = false;
};

struct Menu {
    std::string label;
    std::vector<MenuItem> items;
    int x_offset = 0;
    int width = 0;
    int dropdown_width = 0;
};

class MenuBar {
private:
    std::vector<Menu> menus;
    const Layout* L = nullptr;
    TTF_Font* font = nullptr;
    int active_menu = -1;
    int hovered_menu = -1;
    int hovered_item = -1;
    bool dropdown_open = false;
    MenuContext ctx;

    void init_menus() {
        Menu file_menu;
        file_menu.label = "File";
        file_menu.items = {
            {"Save", "Ctrl+S", MenuAction::Save, false},
            {"Save As...", "", MenuAction::SaveAs, true},
            {"Open File", "", MenuAction::OpenFile, false},
            {"Open Folder", "", MenuAction::OpenFolder, true},
            {"Exit", "Ctrl+Q", MenuAction::Exit, false}
        };
        menus.push_back(file_menu);

        Menu git_menu;
        git_menu.label = "Git";
        git_menu.items = {
            {"Commit...", "Ctrl+K", MenuAction::GitCommit, false},
            {"Pull", "", MenuAction::GitPull, false},
            {"Push", "", MenuAction::GitPush, true},
            {"Checkout...", "", MenuAction::GitCheckout, true},
            {"Reset (Hard)", "", MenuAction::GitResetHard, false}
        };
        menus.push_back(git_menu);

        Menu help_menu;
        help_menu.label = "Help";
        help_menu.items = {
            {"Keymap", "", MenuAction::Keymap, true},
            {"About", "", MenuAction::About, false}
        };
        menus.push_back(help_menu);
    }

    void calculate_positions() {
        int x = L->menu_item_padding;
        constexpr int SHORTCUT_GAP = 30;

        for (auto& menu : menus) {
            int text_w = 0;
            TTF_SizeUTF8(font, menu.label.c_str(), &text_w, nullptr);
            menu.x_offset = x;
            menu.width = text_w + L->menu_item_padding * 2;
            x += menu.width;

            int max_item_w = 0;
            for (const auto& item : menu.items) {
                int label_w = 0, shortcut_w = 0;
                TTF_SizeUTF8(font, item.label.c_str(), &label_w, nullptr);
                if (!item.shortcut.empty()) {
                    TTF_SizeUTF8(font, item.shortcut.c_str(), &shortcut_w, nullptr);
                }
                int total = label_w + (shortcut_w > 0 ? shortcut_w + SHORTCUT_GAP : 0);
                max_item_w = std::max(max_item_w, total);
            }
            menu.dropdown_width = max_item_w + L->menu_item_padding * 2;
        }
    }

    void execute_action(MenuAction action) {
        switch (action) {
            case MenuAction::Save:
                if (ctx.save_file) ctx.save_file();
                break;
            case MenuAction::SaveAs: {
                std::string path = show_save_dialog();
                if (!path.empty() && ctx.save_file_as) {
                    ctx.save_file_as(path);
                }
                break;
            }
            case MenuAction::OpenFile: {
                std::string path = show_open_file_dialog();
                if (!path.empty() && ctx.open_file) {
                    ctx.open_file(path);
                }
                break;
            }
            case MenuAction::OpenFolder: {
                std::string path = show_open_folder_dialog();
                if (!path.empty() && ctx.open_folder) {
                    ctx.open_folder(path);
                }
                break;
            }
            case MenuAction::Exit:
                if (ctx.exit_app) ctx.exit_app();
                break;
            case MenuAction::About:
                if (ctx.open_virtual_file) ctx.open_virtual_file("About", HelpContent::get_about());
                break;
            case MenuAction::Keymap:
                if (ctx.open_virtual_file) ctx.open_virtual_file("Keymap", HelpContent::KEYMAP);
                break;
            case MenuAction::GitCommit:
                if (ctx.git_commit) ctx.git_commit();
                break;
            case MenuAction::GitPull:
                if (ctx.git_pull) ctx.git_pull();
                break;
            case MenuAction::GitPush:
                if (ctx.git_push) ctx.git_push();
                break;
            case MenuAction::GitResetHard:
                if (ctx.git_reset_hard) ctx.git_reset_hard();
                break;
            case MenuAction::GitCheckout:
                if (ctx.git_checkout) ctx.git_checkout();
                break;
            default:
                break;
        }
    }

public:
    void set_context(MenuContext context) { ctx = std::move(context); }

    void set_layout(const Layout* layout) {
        L = layout;
        if (menus.empty()) {
            init_menus();
        }
        if (font) {
            calculate_positions();
        }
    }

    void set_font(TTF_Font* f) {
        font = f;
        if (L && menus.empty()) {
            init_menus();
        }
        if (L) {
            calculate_positions();
        }
    }

    bool is_open() const { return dropdown_open; }

    void close() {
        dropdown_open = false;
        active_menu = -1;
        hovered_item = -1;
    }

    bool handle_mouse_click(int mouse_x, int mouse_y) {
        if (mouse_y >= 0 && mouse_y < L->menu_bar_height) {
            for (int i = 0; i < static_cast<int>(menus.size()); i++) {
                const Menu& menu = menus[i];
                if (mouse_x >= menu.x_offset && mouse_x < menu.x_offset + menu.width) {
                    if (dropdown_open && active_menu == i) {
                        close();
                    } else {
                        dropdown_open = true;
                        active_menu = i;
                        hovered_item = -1;
                    }
                    return true;
                }
            }
            if (dropdown_open) {
                close();
            }
            return false;
        }

        if (dropdown_open && active_menu >= 0 && active_menu < static_cast<int>(menus.size())) {
            const Menu& menu = menus[active_menu];
            int dropdown_x = menu.x_offset;
            int dropdown_y = L->menu_bar_height;
            int item_y = dropdown_y;

            for (int i = 0; i < static_cast<int>(menu.items.size()); i++) {
                const MenuItem& item = menu.items[i];
                int item_h = L->menu_dropdown_item_height;

                if (mouse_x >= dropdown_x && mouse_x < dropdown_x + menu.dropdown_width &&
                    mouse_y >= item_y && mouse_y < item_y + item_h) {
                    MenuAction action = item.action;
                    close();
                    execute_action(action);
                    return true;
                }

                item_y += item_h;
                if (item.separator_after) {
                    item_y += L->scaled(6);
                }
            }
        }

        if (dropdown_open) {
            close();
            return true;
        }
        return false;
    }

    void handle_mouse_motion(int mouse_x, int mouse_y) {
        hovered_menu = -1;
        hovered_item = -1;

        if (mouse_y >= 0 && mouse_y < L->menu_bar_height) {
            for (int i = 0; i < static_cast<int>(menus.size()); i++) {
                const Menu& menu = menus[i];
                if (mouse_x >= menu.x_offset && mouse_x < menu.x_offset + menu.width) {
                    hovered_menu = i;
                    if (dropdown_open && active_menu != i) {
                        active_menu = i;
                    }
                    return;
                }
            }
            return;
        }

        if (dropdown_open && active_menu >= 0 && active_menu < static_cast<int>(menus.size())) {
            const Menu& menu = menus[active_menu];
            int dropdown_x = menu.x_offset;
            int dropdown_y = L->menu_bar_height;
            int item_y = dropdown_y;

            for (int i = 0; i < static_cast<int>(menu.items.size()); i++) {
                const MenuItem& item = menu.items[i];
                int item_h = L->menu_dropdown_item_height;

                if (mouse_x >= dropdown_x && mouse_x < dropdown_x + menu.dropdown_width &&
                    mouse_y >= item_y && mouse_y < item_y + item_h) {
                    hovered_item = i;
                    return;
                }

                item_y += item_h;
                if (item.separator_after) {
                    item_y += L->scaled(6);
                }
            }
        }
    }

    void render(SDL_Renderer* renderer, TextureCache& texture_cache, int window_w, int line_height) {
        SDL_SetRenderDrawColor(renderer, MENU_BAR_BG.r, MENU_BAR_BG.g, MENU_BAR_BG.b, 255);
        SDL_Rect bar_bg = {0, 0, window_w, L->menu_bar_height};
        SDL_RenderFillRect(renderer, &bar_bg);

        SDL_SetRenderDrawColor(renderer, MENU_SEPARATOR.r, MENU_SEPARATOR.g, MENU_SEPARATOR.b, 255);
        SDL_RenderDrawLine(renderer, 0, L->menu_bar_height - 1, window_w, L->menu_bar_height - 1);

        for (int i = 0; i < static_cast<int>(menus.size()); i++) {
            const Menu& menu = menus[i];
            bool is_active = (dropdown_open && active_menu == i);
            bool is_hovered = (hovered_menu == i);

            if (is_active) {
                SDL_SetRenderDrawColor(renderer, MENU_ITEM_ACTIVE.r, MENU_ITEM_ACTIVE.g, MENU_ITEM_ACTIVE.b, 255);
                SDL_Rect item_bg = {menu.x_offset, 0, menu.width, L->menu_bar_height};
                SDL_RenderFillRect(renderer, &item_bg);
            } else if (is_hovered) {
                SDL_SetRenderDrawColor(renderer, MENU_ITEM_HOVER.r, MENU_ITEM_HOVER.g, MENU_ITEM_HOVER.b, 255);
                SDL_Rect item_bg = {menu.x_offset, 0, menu.width, L->menu_bar_height};
                SDL_RenderFillRect(renderer, &item_bg);
            }

            int text_y = (L->menu_bar_height - line_height) / 2;
            texture_cache.render_cached_text(menu.label.c_str(), MENU_TEXT,
                                             menu.x_offset + L->menu_item_padding, text_y);
        }
    }

    void render_dropdown_overlay(SDL_Renderer* renderer, TextureCache& texture_cache, int line_height) {
        if (dropdown_open && active_menu >= 0 && active_menu < static_cast<int>(menus.size())) {
            render_dropdown(renderer, texture_cache, line_height);
        }
    }

private:
    void render_dropdown(SDL_Renderer* renderer, TextureCache& texture_cache, int line_height) {
        const Menu& menu = menus[active_menu];
        int dropdown_x = menu.x_offset;
        int dropdown_y = L->menu_bar_height;

        int dropdown_height = 0;
        for (const auto& item : menu.items) {
            dropdown_height += L->menu_dropdown_item_height;
            if (item.separator_after) {
                dropdown_height += L->scaled(6);
            }
        }

        SDL_SetRenderDrawColor(renderer, MENU_DROPDOWN_BG.r, MENU_DROPDOWN_BG.g, MENU_DROPDOWN_BG.b, 255);
        SDL_Rect dropdown_bg = {dropdown_x, dropdown_y, menu.dropdown_width, dropdown_height};
        SDL_RenderFillRect(renderer, &dropdown_bg);

        SDL_SetRenderDrawColor(renderer, MENU_SEPARATOR.r, MENU_SEPARATOR.g, MENU_SEPARATOR.b, 255);
        SDL_RenderDrawRect(renderer, &dropdown_bg);

        int item_y = dropdown_y;
        for (int i = 0; i < static_cast<int>(menu.items.size()); i++) {
            const MenuItem& item = menu.items[i];
            bool is_hovered = (hovered_item == i);

            if (is_hovered) {
                SDL_SetRenderDrawColor(renderer, MENU_DROPDOWN_HOVER.r, MENU_DROPDOWN_HOVER.g,
                                       MENU_DROPDOWN_HOVER.b, 255);
                SDL_Rect item_bg = {dropdown_x + 1, item_y, menu.dropdown_width - 2, L->menu_dropdown_item_height};
                SDL_RenderFillRect(renderer, &item_bg);
            }

            int text_y = item_y + (L->menu_dropdown_item_height - line_height) / 2;
            texture_cache.render_cached_text(item.label.c_str(), MENU_TEXT,
                                             dropdown_x + L->menu_item_padding, text_y);

            if (!item.shortcut.empty()) {
                int shortcut_x = dropdown_x + menu.dropdown_width - L->menu_item_padding;
                texture_cache.render_cached_text_right_aligned(item.shortcut.c_str(), MENU_TEXT_DIM,
                                                               shortcut_x, text_y);
            }

            item_y += L->menu_dropdown_item_height;

            if (item.separator_after) {
                int sep_y = item_y + L->scaled(3);
                SDL_SetRenderDrawColor(renderer, MENU_SEPARATOR.r, MENU_SEPARATOR.g, MENU_SEPARATOR.b, 255);
                SDL_RenderDrawLine(renderer, dropdown_x + L->scaled(8), sep_y,
                                   dropdown_x + menu.dropdown_width - L->scaled(8), sep_y);
                item_y += L->scaled(6);
            }
        }
    }
};
