#pragma once

#include <SDL2/SDL_ttf.h>
#include <functional>
#include <string>
#include <filesystem>
#include "Constants.h"
#include "Utils.h"

class FontManager {
private:
    TTF_Font* font = nullptr;
    std::string font_path_str;
    int font_size = DEFAULT_FONT_SIZE;
    int line_height = 0;
    int char_width = 0;
    std::function<void()> on_font_changed;

    std::string find_font_path() {
        std::string resource_font = get_resource_path(FONT_NAME);
        if (std::filesystem::exists(resource_font)) {
            return resource_font;
        }

        for (int i = 0; FONT_SEARCH_PATHS[i] != nullptr; i++) {
            if (std::filesystem::exists(FONT_SEARCH_PATHS[i])) {
                return FONT_SEARCH_PATHS[i];
            }
        }

        const char* home = getenv("HOME");
        if (home) {
            std::string user_font = std::string(home) + "/.local/share/fonts/" + FONT_NAME;
            if (std::filesystem::exists(user_font)) {
                return user_font;
            }
            
            user_font = std::string(home) + "/Library/Fonts/" + FONT_NAME;
            if (std::filesystem::exists(user_font)) {
                return user_font;
            }
        }

        return "";
    }

public:
    ~FontManager() {
        close();
    }

    void close() {
        if (font) {
            TTF_CloseFont(font);
            font = nullptr;
        }
    }

    bool init(int size) {
        font_path_str = find_font_path();
        if (font_path_str.empty()) {
            SDL_Log("Could not find any suitable font. Searched locations:");
            char* base_path = SDL_GetBasePath();
            if (base_path) {
                SDL_Log("  - %s%s (bundled)", base_path, FONT_NAME);
                SDL_free(base_path);
            }
            for (int i = 0; FONT_SEARCH_PATHS[i] != nullptr; i++) {
                SDL_Log("  - %s", FONT_SEARCH_PATHS[i]);
            }
            return false;
        }

        font_size = size;
        font = TTF_OpenFont(font_path_str.c_str(), size);
        if (font) {
            SDL_Log("Loaded font: %s", font_path_str.c_str());
            update_metrics();
            return true;
        }
        return false;
    }

    bool init(const char* path, int size) {
        font_path_str = path;
        font_size = size;
        font = TTF_OpenFont(path, size);
        if (font) {
            update_metrics();
            return true;
        }
        return false;
    }

    void set_on_font_changed(std::function<void()> callback) {
        on_font_changed = std::move(callback);
    }

    bool change_size(int new_size) {
        if (new_size < MIN_FONT_SIZE || new_size > MAX_FONT_SIZE) {
            return false;
        }
        if (TTF_Font* new_font = TTF_OpenFont(font_path_str.c_str(), new_size)) {
            TTF_CloseFont(font);
            font = new_font;
            font_size = new_size;
            update_metrics();
            if (on_font_changed) {
                on_font_changed();
            }
            return true;
        }
        return false;
    }

    void increase_size() {
        if (font_size < MAX_FONT_SIZE) {
            change_size(font_size + 2);
        }
    }

    void decrease_size() {
        if (font_size > MIN_FONT_SIZE) {
            change_size(font_size - 2);
        }
    }

    void reset_size() {
        change_size(DEFAULT_FONT_SIZE);
    }

    TTF_Font* get() const { return font; }
    int get_size() const { return font_size; }
    int get_line_height() const { return line_height; }
    int get_char_width() const { return char_width; }

private:
    void update_metrics() {
        line_height = TTF_FontLineSkip(font);
        TTF_SizeUTF8(font, "M", &char_width, nullptr);
    }
};
