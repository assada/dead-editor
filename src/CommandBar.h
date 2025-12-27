#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string>
#include <cstdio>
#include <functional>
#include "Constants.h"
#include "Layout.h"
#include "Utils.h"
#include "RenderUtils.h"
#include "TextureCache.h"

enum class CommandMode { None, Search, GoTo, Create, Delete, SavePrompt };

enum class CommandAction { None, Confirm, Cancel, FindNext };

struct EditorStatus {
    std::string file_path;
    bool modified = false;
    int cursor_line = 0;
    int cursor_col = 0;
    int total_lines = 0;
};

struct CommandKeyResult {
    CommandAction action = CommandAction::None;
    CommandMode mode = CommandMode::None;
    std::string path;
    std::string input;
    int line = 0;
    int col = 0;
};

class CommandBar {
private:
    CommandMode mode = CommandMode::None;
    std::string input;
    std::string base_path;
    std::string target_name;
    std::string last_search;
    bool just_confirmed = false;
    const Layout* L = nullptr;

public:
    void set_layout(const Layout* layout) { L = layout; }
    bool is_active() const { return mode != CommandMode::None; }
    CommandMode get_mode() const { return mode; }
    const std::string& get_input() const { return input; }
    const std::string& get_base_path() const { return base_path; }
    const std::string& get_target_name() const { return target_name; }
    const std::string& get_search_query() const {
        return (mode == CommandMode::Search) ? input : last_search;
    }
    bool was_just_confirmed() const { return just_confirmed; }
    void clear_just_confirmed() { just_confirmed = false; }

    void start_search() {
        mode = CommandMode::Search;
        input.clear();
    }

    void start_goto() {
        mode = CommandMode::GoTo;
        input.clear();
    }

    void start_create(const std::string& path) {
        mode = CommandMode::Create;
        input.clear();
        base_path = path;
    }

    void start_delete(const std::string& path, const std::string& name) {
        mode = CommandMode::Delete;
        input.clear();
        base_path = path;
        target_name = name;
    }

    void start_save_prompt(const std::string& filename) {
        mode = CommandMode::SavePrompt;
        input.clear();
        target_name = filename;
    }

    void cancel() {
        if (mode == CommandMode::Delete || mode == CommandMode::SavePrompt) {
            just_confirmed = true;
        }
        if (mode == CommandMode::Search && !input.empty()) {
            last_search = input;
        }
        mode = CommandMode::None;
        input.clear();
        base_path.clear();
        target_name.clear();
    }

    void confirm_save_prompt() {
        just_confirmed = true;
        mode = CommandMode::None;
        target_name.clear();
    }

    void confirm_delete() {
        just_confirmed = true;
        mode = CommandMode::None;
        base_path.clear();
        target_name.clear();
    }

    void confirm_and_close() {
        mode = CommandMode::None;
        input.clear();
        base_path.clear();
    }

    bool handle_text_input(const char* text) {
        if (mode == CommandMode::Delete || mode == CommandMode::SavePrompt || just_confirmed) {
            return true;
        }
        if (mode != CommandMode::None) {
            input += text;
            return true;
        }
        return false;
    }

    bool handle_backspace() {
        if (mode == CommandMode::None || mode == CommandMode::Delete) {
            return false;
        }
        if (!input.empty()) {
            int prev = utf8_prev_char_start(input, static_cast<int>(input.size()));
            input = input.substr(0, prev);
        }
        return true;
    }

    CommandKeyResult handle_key(const SDL_Event& event) {
        CommandKeyResult result;
        result.mode = mode;

        if (mode == CommandMode::Delete) {
            switch (event.key.keysym.sym) {
                case SDLK_y:
                    result.action = CommandAction::Confirm;
                    result.path = base_path;
                    confirm_delete();
                    break;
                case SDLK_n:
                case SDLK_ESCAPE:
                    result.action = CommandAction::Cancel;
                    cancel();
                    break;
            }
            return result;
        }

        if (mode == CommandMode::SavePrompt) {
            switch (event.key.keysym.sym) {
                case SDLK_y:
                    result.action = CommandAction::Confirm;
                    result.input = "save";
                    confirm_save_prompt();
                    break;
                case SDLK_n:
                    result.action = CommandAction::Confirm;
                    result.input = "discard";
                    confirm_save_prompt();
                    break;
                case SDLK_c:
                case SDLK_ESCAPE:
                    result.action = CommandAction::Cancel;
                    cancel();
                    break;
            }
            return result;
        }

        if (mode == CommandMode::Create) {
            switch (event.key.keysym.sym) {
                case SDLK_ESCAPE:
                    result.action = CommandAction::Cancel;
                    cancel();
                    break;
                case SDLK_RETURN:
                    if (!input.empty()) {
                        result.action = CommandAction::Confirm;
                        result.path = base_path;
                        result.input = input;
                    }
                    confirm_and_close();
                    break;
                case SDLK_BACKSPACE:
                    handle_backspace();
                    break;
            }
            return result;
        }

        if (mode == CommandMode::GoTo) {
            switch (event.key.keysym.sym) {
                case SDLK_ESCAPE:
                    result.action = CommandAction::Cancel;
                    cancel();
                    break;
                case SDLK_RETURN:
                    if (!input.empty()) {
                        result.action = CommandAction::Confirm;
                        parse_goto_input(input, result.line, result.col);
                    }
                    confirm_and_close();
                    break;
                case SDLK_BACKSPACE:
                    handle_backspace();
                    break;
            }
            return result;
        }

        if (mode == CommandMode::Search) {
            switch (event.key.keysym.sym) {
                case SDLK_ESCAPE:
                    result.action = CommandAction::Cancel;
                    cancel();
                    break;
                case SDLK_RETURN:
                case SDLK_F3:
                    if (!input.empty()) {
                        result.action = CommandAction::FindNext;
                        result.input = input;
                    }
                    break;
                case SDLK_BACKSPACE:
                    handle_backspace();
                    break;
            }
            return result;
        }

        return result;
    }

    void render(SDL_Renderer* renderer, TTF_Font* font, TextureCache& texture_cache,
                int x, int y, int width, int line_height, bool cursor_visible) {
        if (mode == CommandMode::None) return;

        SDL_Color bg_color = (mode == CommandMode::Delete || mode == CommandMode::SavePrompt)
            ? SDL_Color{80, 60, 40, 255}
            : Colors::SEARCH_BG;
        SDL_SetRenderDrawColor(renderer, bg_color.r, bg_color.g, bg_color.b, 255);
        SDL_Rect bar = {x, y, width, L->search_bar_height};
        SDL_RenderFillRect(renderer, &bar);

        int text_y = y + (L->search_bar_height - line_height) / 2;
        const char* label = get_label_buf();
        texture_cache.render_cached_text(label, Colors::TEXT, x + L->padding, text_y);

        if (cursor_visible && mode != CommandMode::Delete && mode != CommandMode::SavePrompt) {
            int label_w = 0;
            TTF_SizeUTF8(font, label, &label_w, nullptr);
            SDL_SetRenderDrawColor(renderer, Colors::CURSOR.r, Colors::CURSOR.g, Colors::CURSOR.b, 255);
            SDL_Rect cursor = {x + L->padding + label_w, text_y, L->scaled(2), line_height};
            SDL_RenderFillRect(renderer, &cursor);
        }
    }

    void render_status_bar(SDL_Renderer* renderer, TextureCache& texture_cache,
                           int x, int y, int width, int line_height,
                           const EditorStatus& status, const std::string& git_branch) {
        SDL_SetRenderDrawColor(renderer, Colors::GUTTER.r, Colors::GUTTER.g, Colors::GUTTER.b, 255);
        SDL_Rect status_bar = {x, y, width, L->status_bar_height};
        SDL_RenderFillRect(renderer, &status_bar);

        char status_buf[512];
        const char* filename = status.file_path.empty() ? "Untitled" : status.file_path.c_str();
        snprintf(status_buf, sizeof(status_buf), "%s%s    Ln %d/%d    Col %d",
                 filename, status.modified ? " *" : "",
                 status.cursor_line + 1, status.total_lines, status.cursor_col + 1);

        int text_y = y + (L->status_bar_height - line_height) / 2;
        texture_cache.render_cached_text(status_buf, Colors::LINE_NUM, x + L->padding, text_y);

        if (!git_branch.empty()) {
            char branch_buf[128];
            snprintf(branch_buf, sizeof(branch_buf), " %s", git_branch.c_str());
            texture_cache.render_cached_text_right_aligned(branch_buf, Colors::GIT_BRANCH, x + width - L->padding, text_y);
        }
    }

private:
    mutable char label_buf[256];

    const char* get_label_buf() const {
        switch (mode) {
            case CommandMode::Search:
                snprintf(label_buf, sizeof(label_buf), "Find: %s", input.c_str());
                break;
            case CommandMode::GoTo:
                snprintf(label_buf, sizeof(label_buf), "Go to (line:col): %s", input.c_str());
                break;
            case CommandMode::Create:
                snprintf(label_buf, sizeof(label_buf), "New: %s", input.c_str());
                break;
            case CommandMode::Delete:
                snprintf(label_buf, sizeof(label_buf), "Delete '%s'? (y/n)", target_name.c_str());
                break;
            case CommandMode::SavePrompt:
                snprintf(label_buf, sizeof(label_buf), "Save changes to '%s'? (y)es / (n)o / (c)ancel", target_name.c_str());
                break;
            default:
                label_buf[0] = '\0';
                break;
        }
        return label_buf;
    }
};
