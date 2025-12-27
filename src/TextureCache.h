#pragma once

#include "Types.h"
#include "HandleTypes.h"
#include "LRUCache.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string>
#include <vector>
#include <functional>

struct CachedLineRender {
    std::string content;
    std::vector<Token> tokens;
    TexturePtr texture;
    int width = 0;
    int height = 0;
    bool valid = false;

    void reset();
    bool matches(const std::string& text, const std::vector<Token>& toks) const;
};

struct CachedTexture {
    TexturePtr texture;
    int width = 0;
    int height = 0;
};

using LineRenderCache = LRUCache<size_t, CachedLineRender>;

struct TextureCache {
    static constexpr size_t MAX_CACHED_TEXT = 500;
    static constexpr size_t MAX_LINE_NUMBERS = 1000;

    LRUCache<uint64_t, CachedTexture> text_cache{MAX_CACHED_TEXT};
    LRUCache<std::string, CachedTexture> line_number_cache{MAX_LINE_NUMBERS};
    SDL_Renderer* renderer = nullptr;
    TTF_Font* font = nullptr;
    int font_version = 0;
    int line_height = 0;

    void init(SDL_Renderer* r, TTF_Font* f);
    void invalidate_all();
    void set_font(TTF_Font* f);

    uint64_t make_text_key(const std::string& text, SDL_Color color);
    void render_cached_text(const std::string& text, SDL_Color color, int x, int y);
    void render_cached_text_right_aligned(const std::string& text, SDL_Color color, int right_x, int y);
    SDL_Texture* get_line_number_texture(const std::string& num_str, SDL_Color color, int& w, int& h);

    SDL_Surface* render_text_to_surface(const std::string& text, SDL_Color color);
    SDL_Surface* render_line_to_surface(
        const std::string& line_text,
        const std::vector<Token>& tokens,
        SDL_Color default_color,
        const std::function<SDL_Color(TokenType)>& get_color
    );

    ~TextureCache();
};

CachedLineRender& build_line_render(
    LineRenderCache& cache,
    size_t line_idx,
    const std::string& line_text,
    const std::vector<Token>& tokens,
    SDL_Renderer* renderer,
    TTF_Font* font,
    int line_height,
    SDL_Color default_color,
    const std::function<SDL_Color(TokenType)>& get_color
);

void render_line(const CachedLineRender& cached, SDL_Renderer* renderer, int x, int y);
