#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <vector>
#include <string>
#include <memory>
#include <filesystem>
#include <algorithm>
#include "Editor.h"
#include "Constants.h"
#include "Layout.h"
#include "TextureCache.h"
#include "FileTree.h"

struct Tab {
    std::unique_ptr<Editor> editor;
    std::string title;

    Tab() : editor(std::make_unique<Editor>()) {}

    bool is_modified() const { return editor && editor->is_modified(); }

    const std::string& get_path() const {
        static std::string empty;
        return editor ? editor->get_file_path() : empty;
    }

    void update_title() {
        if (editor && !editor->get_file_path().empty()) {
            title = std::filesystem::path(editor->get_file_path()).filename().string();
        } else {
            title = "Untitled";
        }
    }
};

enum class TabAction {
    None,
    SwitchTab,
    CloseTab,
    CloseModifiedTab
};

struct TabClickResult {
    TabAction action = TabAction::None;
    int tab_index = -1;
};

class TabBar {
private:
    std::vector<Tab> tabs;
    int active_tab = -1;
    int hovered_tab = -1;
    int hovered_close = -1;
    int scroll_offset = 0;
    int tab_pending_close = -1;

    const Layout* L = nullptr;
    TTF_Font* font = nullptr;

    int get_tab_width(const Tab& tab) const {
        int text_w = 0;
        TTF_SizeUTF8(font, tab.title.c_str(), &text_w, nullptr);
        return L->tab_padding * 2 + text_w + L->tab_close_size + L->tab_close_padding;
    }

    int get_total_tabs_width() const {
        int total = 0;
        for (const auto& t : tabs) {
            total += get_tab_width(t);
        }
        return total;
    }

public:
    void set_layout(const Layout* layout) { L = layout; }
    void set_font(TTF_Font* f) { font = f; }

    bool has_tabs() const { return !tabs.empty(); }
    int get_active_index() const { return active_tab; }
    int get_tab_count() const { return static_cast<int>(tabs.size()); }
    int get_pending_close_tab() const { return tab_pending_close; }
    void clear_pending_close() { tab_pending_close = -1; }

    Editor* get_active_editor() {
        if (active_tab >= 0 && active_tab < static_cast<int>(tabs.size())) {
            return tabs[active_tab].editor.get();
        }
        return nullptr;
    }

    const Tab* get_tab(int index) const {
        if (index >= 0 && index < static_cast<int>(tabs.size())) {
            return &tabs[index];
        }
        return nullptr;
    }

    Tab* get_tab_mut(int index) {
        if (index >= 0 && index < static_cast<int>(tabs.size())) {
            return &tabs[index];
        }
        return nullptr;
    }

    int find_tab_by_path(const std::string& path) const {
        for (int i = 0; i < static_cast<int>(tabs.size()); i++) {
            if (tabs[i].get_path() == path) {
                return i;
            }
        }
        return -1;
    }

    int open_file(const std::string& path, int line_height, bool force_reload = false) {
        int existing = find_tab_by_path(path);
        if (existing >= 0) {
            active_tab = existing;
            if (force_reload && tabs[existing].editor && !tabs[existing].is_modified()) {
                tabs[existing].editor->load_file(path.c_str());
                tabs[existing].update_title();
            }
            return existing;
        }

        Tab new_tab;
        new_tab.editor->set_line_height(line_height);
        if (!new_tab.editor->load_file(path.c_str())) {
            return -1;
        }
        new_tab.update_title();

        tabs.push_back(std::move(new_tab));
        active_tab = static_cast<int>(tabs.size()) - 1;
        return active_tab;
    }

    int create_new_tab(int line_height) {
        Tab new_tab;
        new_tab.editor->set_line_height(line_height);
        new_tab.title = "Untitled";
        tabs.push_back(std::move(new_tab));
        active_tab = static_cast<int>(tabs.size()) - 1;
        return active_tab;
    }

    int open_virtual_file(const std::string& title, const std::string& content, int line_height) {
        for (int i = 0; i < static_cast<int>(tabs.size()); i++) {
            if (tabs[i].title == title && tabs[i].get_path().empty()) {
                active_tab = i;
                return i;
            }
        }

        Tab new_tab;
        new_tab.editor->set_line_height(line_height);
        new_tab.editor->load_text(content);
        new_tab.editor->set_readonly(true);
        new_tab.title = title;
        tabs.push_back(std::move(new_tab));
        active_tab = static_cast<int>(tabs.size()) - 1;
        return active_tab;
    }

    bool close_tab(int index) {
        if (index < 0 || index >= static_cast<int>(tabs.size())) {
            return false;
        }

        tabs.erase(tabs.begin() + index);

        if (tabs.empty()) {
            active_tab = -1;
        } else if (active_tab >= static_cast<int>(tabs.size())) {
            active_tab = static_cast<int>(tabs.size()) - 1;
        } else if (active_tab > index) {
            active_tab--;
        }

        return true;
    }

    bool try_close_tab(int index) {
        if (index < 0 || index >= static_cast<int>(tabs.size())) {
            return false;
        }

        if (tabs[index].is_modified()) {
            tab_pending_close = index;
            return false;
        }

        return close_tab(index);
    }

    void switch_to_tab(int index) {
        if (index >= 0 && index < static_cast<int>(tabs.size())) {
            active_tab = index;
        }
    }

    void next_tab() {
        if (tabs.empty()) return;
        active_tab = (active_tab + 1) % static_cast<int>(tabs.size());
    }

    void prev_tab() {
        if (tabs.empty()) return;
        active_tab = (active_tab - 1 + static_cast<int>(tabs.size())) % static_cast<int>(tabs.size());
    }

    void update_active_title() {
        if (active_tab >= 0 && active_tab < static_cast<int>(tabs.size())) {
            tabs[active_tab].update_title();
        }
    }

    TabClickResult handle_mouse_click(int mouse_x, int mouse_y) {
        TabClickResult result;

        if (mouse_y < 0 || mouse_y >= L->tab_bar_height) {
            return result;
        }

        int x = -scroll_offset;
        for (int i = 0; i < static_cast<int>(tabs.size()); i++) {
            int tab_w = get_tab_width(tabs[i]);

            if (mouse_x >= x && mouse_x < x + tab_w) {
                int close_x = x + tab_w - L->tab_close_size - L->tab_close_padding / 2;
                int close_y = (L->tab_bar_height - L->tab_close_size) / 2;

                if (mouse_x >= close_x && mouse_x < close_x + L->tab_close_size &&
                    mouse_y >= close_y && mouse_y < close_y + L->tab_close_size) {
                    if (tabs[i].is_modified()) {
                        result.action = TabAction::CloseModifiedTab;
                    } else {
                        result.action = TabAction::CloseTab;
                    }
                    result.tab_index = i;
                } else {
                    result.action = TabAction::SwitchTab;
                    result.tab_index = i;
                }
                return result;
            }

            x += tab_w;
        }

        return result;
    }

    void handle_mouse_motion(int mouse_x, int mouse_y) {
        hovered_tab = -1;
        hovered_close = -1;

        if (mouse_y < 0 || mouse_y >= L->tab_bar_height) {
            return;
        }

        int x = -scroll_offset;
        for (int i = 0; i < static_cast<int>(tabs.size()); i++) {
            int tab_w = get_tab_width(tabs[i]);

            if (mouse_x >= x && mouse_x < x + tab_w) {
                hovered_tab = i;

                int close_x = x + tab_w - L->tab_close_size - L->tab_close_padding / 2;
                int close_y = (L->tab_bar_height - L->tab_close_size) / 2;

                if (mouse_x >= close_x && mouse_x < close_x + L->tab_close_size &&
                    mouse_y >= close_y && mouse_y < close_y + L->tab_close_size) {
                    hovered_close = i;
                }
                return;
            }

            x += tab_w;
        }
    }

    void handle_scroll(int wheel_y, int bar_width) {
        int total_width = get_total_tabs_width();
        int max_scroll = std::max(0, total_width - bar_width);

        scroll_offset = std::clamp(scroll_offset - wheel_y * L->scaled(30), 0, max_scroll);
    }

    void ensure_tab_visible(int bar_width) {
        if (active_tab < 0 || tabs.empty()) return;

        int x = 0;
        for (int i = 0; i < active_tab; i++) {
            x += get_tab_width(tabs[i]);
        }
        int tab_w = get_tab_width(tabs[active_tab]);

        if (x < scroll_offset) {
            scroll_offset = x;
        } else if (x + tab_w > scroll_offset + bar_width) {
            scroll_offset = x + tab_w - bar_width;
        }
    }

    void render(SDL_Renderer* renderer, TextureCache& texture_cache,
                int x_offset, int y_offset, int bar_width, int line_height,
                const FileTree* file_tree = nullptr) {

        SDL_SetRenderDrawColor(renderer, TAB_BG_COLOR.r, TAB_BG_COLOR.g, TAB_BG_COLOR.b, 255);
        SDL_Rect bar_bg = {x_offset, y_offset, bar_width, L->tab_bar_height};
        SDL_RenderFillRect(renderer, &bar_bg);

        SDL_RenderSetClipRect(renderer, &bar_bg);

        int x = x_offset - scroll_offset;
        for (int i = 0; i < static_cast<int>(tabs.size()); i++) {
            const Tab& tab = tabs[i];
            int tab_w = get_tab_width(tab);

            SDL_Color bg = (i == active_tab) ? TAB_ACTIVE_COLOR :
                           (i == hovered_tab) ? TAB_HOVER_COLOR : TAB_BG_COLOR;

            SDL_SetRenderDrawColor(renderer, bg.r, bg.g, bg.b, 255);
            SDL_Rect tab_rect = {x, y_offset, tab_w, L->tab_bar_height};
            SDL_RenderFillRect(renderer, &tab_rect);

            if (i == active_tab) {
                SDL_SetRenderDrawColor(renderer, TAB_ACTIVE_INDICATOR.r, TAB_ACTIVE_INDICATOR.g,
                                       TAB_ACTIVE_INDICATOR.b, 255);
                SDL_Rect indicator = {x, y_offset + L->tab_bar_height - L->scaled(2), tab_w, L->scaled(2)};
                SDL_RenderFillRect(renderer, &indicator);
            }

            SDL_SetRenderDrawColor(renderer, TAB_BORDER_COLOR.r, TAB_BORDER_COLOR.g,
                                   TAB_BORDER_COLOR.b, 255);
            SDL_RenderDrawLine(renderer, x + tab_w - 1, y_offset + L->scaled(4),
                               x + tab_w - 1, y_offset + L->tab_bar_height - L->scaled(4));

            int text_x = x + L->tab_padding;
            int text_y = y_offset + (L->tab_bar_height - line_height) / 2;

            if (tab.is_modified()) {
                SDL_SetRenderDrawColor(renderer, TAB_MODIFIED_DOT.r, TAB_MODIFIED_DOT.g,
                                       TAB_MODIFIED_DOT.b, 255);
                int dot_y = y_offset + L->tab_bar_height / 2;
                int dot_size = L->scaled(6);
                SDL_Rect dot = {text_x, dot_y - dot_size / 2, dot_size, dot_size};
                SDL_RenderFillRect(renderer, &dot);
                text_x += L->scaled(10);
            }

            SDL_Color text_color = (i == active_tab) ? TAB_TEXT_ACTIVE : TAB_TEXT_INACTIVE;
            if (file_tree && !tab.get_path().empty()) {
                if (file_tree->is_file_untracked(tab.get_path())) {
                    text_color = Colors::GIT_UNTRACKED;
                } else if (file_tree->is_file_modified(tab.get_path())) {
                    text_color = Colors::GIT_MODIFIED;
                } else if (file_tree->is_file_added(tab.get_path())) {
                    text_color = Colors::GIT_ADDED;
                }
            }
            texture_cache.render_cached_text(tab.title.c_str(), text_color, text_x, text_y);

            int close_x = x + tab_w - L->tab_close_size - L->tab_close_padding / 2;
            int close_y = y_offset + (L->tab_bar_height - L->tab_close_size) / 2;

            if (i == hovered_close) {
                SDL_SetRenderDrawColor(renderer, TAB_CLOSE_HOVER_BG.r, TAB_CLOSE_HOVER_BG.g,
                                       TAB_CLOSE_HOVER_BG.b, 255);
                SDL_Rect close_bg = {close_x - L->scaled(2), close_y - L->scaled(2),
                                     L->tab_close_size + L->scaled(4), L->tab_close_size + L->scaled(4)};
                SDL_RenderFillRect(renderer, &close_bg);
            }

            SDL_Color close_color = (i == hovered_close) ? TAB_CLOSE_COLOR_HOVER : TAB_CLOSE_COLOR;
            SDL_SetRenderDrawColor(renderer, close_color.r, close_color.g, close_color.b, 255);

            int cx = close_x + L->tab_close_size / 2;
            int cy = close_y + L->tab_close_size / 2;
            int half = L->tab_close_size / 3;
            SDL_RenderDrawLine(renderer, cx - half, cy - half, cx + half, cy + half);
            SDL_RenderDrawLine(renderer, cx + half, cy - half, cx - half, cy + half);

            x += tab_w;
        }

        SDL_RenderSetClipRect(renderer, nullptr);

        SDL_SetRenderDrawColor(renderer, TAB_BORDER_COLOR.r, TAB_BORDER_COLOR.g,
                               TAB_BORDER_COLOR.b, 255);
        SDL_RenderDrawLine(renderer, x_offset, y_offset + L->tab_bar_height - 1,
                           x_offset + bar_width, y_offset + L->tab_bar_height - 1);
    }
};
