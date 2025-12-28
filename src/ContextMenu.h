#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string>
#include <vector>
#include <functional>
#include "Constants.h"
#include "Layout.h"
#include "TextureCache.h"

struct ContextMenuItem {
    std::string label;
    std::function<void()> action;
    bool enabled = true;
    bool separator_after = false;
};

class ContextMenu {
private:
    std::vector<ContextMenuItem> items;
    const Layout* L = nullptr;
    TTF_Font* font = nullptr;
    int pos_x = 0;
    int pos_y = 0;
    int menu_width = 0;
    int hovered_item = -1;
    bool visible = false;

    int calculate_width() const {
        int max_width = L->scaled(120);
        for (const auto& item : items) {
            int text_w = 0;
            TTF_SizeUTF8(font, item.label.c_str(), &text_w, nullptr);
            max_width = std::max(max_width, text_w + L->menu_item_padding * 4);
        }
        return max_width;
    }

    int calculate_height() const {
        int height = 0;
        for (const auto& item : items) {
            height += L->menu_dropdown_item_height;
            if (item.separator_after) {
                height += L->scaled(6);
            }
        }
        return height;
    }

public:
    void set_layout(const Layout* layout) { L = layout; }
    void set_font(TTF_Font* f) { font = f; }

    bool is_open() const { return visible; }

    void show(int x, int y, std::vector<ContextMenuItem>&& menu_items, int window_w, int window_h) {
        items = std::move(menu_items);
        menu_width = calculate_width();
        int menu_height = calculate_height();

        pos_x = x;
        pos_y = y;

        if (pos_x + menu_width > window_w) {
            pos_x = window_w - menu_width;
        }
        if (pos_y + menu_height > window_h) {
            pos_y = window_h - menu_height;
        }

        hovered_item = -1;
        visible = true;
    }

    void close() {
        visible = false;
        items.clear();
        hovered_item = -1;
    }

    bool handle_mouse_click(int mouse_x, int mouse_y) {
        if (!visible) return false;

        int menu_height = calculate_height();

        if (mouse_x >= pos_x && mouse_x < pos_x + menu_width &&
            mouse_y >= pos_y && mouse_y < pos_y + menu_height) {

            int item_y = pos_y;
            for (int i = 0; i < static_cast<int>(items.size()); i++) {
                int item_h = L->menu_dropdown_item_height;

                if (mouse_y >= item_y && mouse_y < item_y + item_h) {
                    if (items[i].enabled && items[i].action) {
                        auto action = items[i].action;
                        close();
                        action();
                    }
                    return true;
                }

                item_y += item_h;
                if (items[i].separator_after) {
                    item_y += L->scaled(6);
                }
            }
            return true;
        }

        close();
        return true;
    }

    void handle_mouse_motion(int mouse_x, int mouse_y) {
        if (!visible) return;

        hovered_item = -1;

        int menu_height = calculate_height();
        if (mouse_x < pos_x || mouse_x >= pos_x + menu_width ||
            mouse_y < pos_y || mouse_y >= pos_y + menu_height) {
            return;
        }

        int item_y = pos_y;
        for (int i = 0; i < static_cast<int>(items.size()); i++) {
            int item_h = L->menu_dropdown_item_height;

            if (mouse_y >= item_y && mouse_y < item_y + item_h) {
                if (items[i].enabled) {
                    hovered_item = i;
                }
                return;
            }

            item_y += item_h;
            if (items[i].separator_after) {
                item_y += L->scaled(6);
            }
        }
    }

    void render(SDL_Renderer* renderer, TextureCache& texture_cache, int line_height) {
        if (!visible || items.empty()) return;

        int menu_height = calculate_height();

        SDL_SetRenderDrawColor(renderer, MENU_DROPDOWN_BG.r, MENU_DROPDOWN_BG.g, MENU_DROPDOWN_BG.b, 255);
        SDL_Rect menu_bg = {pos_x, pos_y, menu_width, menu_height};
        SDL_RenderFillRect(renderer, &menu_bg);

        SDL_SetRenderDrawColor(renderer, MENU_SEPARATOR.r, MENU_SEPARATOR.g, MENU_SEPARATOR.b, 255);
        SDL_RenderDrawRect(renderer, &menu_bg);

        int item_y = pos_y;
        for (int i = 0; i < static_cast<int>(items.size()); i++) {
            const auto& item = items[i];
            bool is_hovered = (hovered_item == i);

            if (is_hovered && item.enabled) {
                SDL_SetRenderDrawColor(renderer, MENU_DROPDOWN_HOVER.r, MENU_DROPDOWN_HOVER.g,
                                       MENU_DROPDOWN_HOVER.b, 255);
                SDL_Rect item_bg = {pos_x + 1, item_y, menu_width - 2, L->menu_dropdown_item_height};
                SDL_RenderFillRect(renderer, &item_bg);
            }

            int text_y = item_y + (L->menu_dropdown_item_height - line_height) / 2;
            SDL_Color text_color = item.enabled ? MENU_TEXT : MENU_TEXT_DIM;
            texture_cache.render_cached_text(item.label.c_str(), text_color,
                                            pos_x + L->menu_item_padding, text_y);

            item_y += L->menu_dropdown_item_height;

            if (item.separator_after) {
                int sep_y = item_y + L->scaled(3);
                SDL_SetRenderDrawColor(renderer, MENU_SEPARATOR.r, MENU_SEPARATOR.g, MENU_SEPARATOR.b, 255);
                SDL_RenderDrawLine(renderer, pos_x + L->scaled(8), sep_y,
                                   pos_x + menu_width - L->scaled(8), sep_y);
                item_y += L->scaled(6);
            }
        }
    }
};
