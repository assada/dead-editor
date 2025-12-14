#pragma once

#include "Types.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string>
#include <vector>
#include <list>
#include <unordered_map>
#include <functional>

struct CachedLineRender {
    std::string content;
    std::vector<Token> tokens;
    SDL_Texture* texture = nullptr;
    int width = 0;
    int height = 0;
    bool valid = false;

    void destroy();
};

struct CachedTexture {
    SDL_Texture* texture = nullptr;
    int width = 0;
    int height = 0;
    std::string content;

    void destroy();
};

struct TextureCache {
    static constexpr size_t MAX_CACHED_LINES = 300;
    static constexpr size_t MAX_CACHED_TEXT = 500;

    std::unordered_map<size_t, CachedLineRender> line_renders;
    std::list<size_t> lru_order;
    std::unordered_map<size_t, std::list<size_t>::iterator> lru_map;
    std::unordered_map<std::string, CachedTexture> line_number_textures;
    std::unordered_map<uint64_t, CachedTexture> text_cache;
    SDL_Renderer* renderer = nullptr;
    TTF_Font* font = nullptr;
    int font_version = 0;
    int line_height = 0;

    void init(SDL_Renderer* r, TTF_Font* f);
    void evict_lru();
    void touch_line(size_t line_idx);
    void invalidate_all();
    void invalidate_line(size_t line_idx);
    void set_font(TTF_Font* f);

    uint64_t make_text_key(const std::string& text, SDL_Color color);
    void render_cached_text(const std::string& text, SDL_Color color, int x, int y);
    void render_cached_text_right_aligned(const std::string& text, SDL_Color color, int right_x, int y);
    SDL_Texture* get_line_number_texture(const std::string& num_str, SDL_Color color, int& w, int& h);

    SDL_Surface* render_line_to_surface(
        const std::string& line_text,
        const std::vector<Token>& tokens,
        SDL_Color default_color,
        const std::function<SDL_Color(TokenType)>& get_color
    );

    CachedLineRender& get_or_build_line_render(
        size_t line_idx,
        const std::string& line_text,
        const std::vector<Token>& tokens,
        SDL_Color default_color,
        const std::function<SDL_Color(TokenType)>& get_color
    );

    void render_cached_line(const CachedLineRender& cached, int x, int y);

    ~TextureCache();
};
