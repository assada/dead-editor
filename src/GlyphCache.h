#pragma once

#include "HandleTypes.h"
#include "LRUCache.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <cstdint>
#include <string>

struct CachedGlyph {
    TexturePtr texture;
    int width = 0;
    int height = 0;
};

struct GlyphKey {
    uint32_t codepoint;
    uint32_t color_packed;
    uint8_t style;

    bool operator==(const GlyphKey& other) const {
        return codepoint == other.codepoint &&
               color_packed == other.color_packed &&
               style == other.style;
    }
};

struct GlyphKeyHash {
    size_t operator()(const GlyphKey& k) const {
        return static_cast<size_t>(
            (static_cast<uint64_t>(k.codepoint) << 32) |
            (static_cast<uint64_t>(k.color_packed) << 1) |
            static_cast<uint64_t>(k.style)
        );
    }
};

class GlyphCache {
public:
    static constexpr size_t DEFAULT_MAX_SIZE = 4096;

    explicit GlyphCache(size_t max_size = DEFAULT_MAX_SIZE)
        : cache_(max_size) {}

    void init(SDL_Renderer* r, TTF_Font* f) {
        renderer_ = r;
        font_ = f;
    }

    void set_font(TTF_Font* f) {
        if (font_ != f) {
            cache_.clear();
            font_ = f;
        }
    }

    CachedGlyph* get(uint32_t codepoint, SDL_Color color, uint8_t style = 0) {
        GlyphKey key{codepoint, pack_color(color), style};
        return cache_.get(key);
    }

    CachedGlyph* get_or_create(uint32_t codepoint, SDL_Color color, uint8_t style = 0) {
        GlyphKey key{codepoint, pack_color(color), style};

        if (auto* cached = cache_.get(key)) {
            return cached;
        }

        CachedGlyph& glyph = cache_.get_or_create(key);
        render_glyph(glyph, codepoint, color);
        return &glyph;
    }

    void clear() {
        cache_.clear();
    }

    static uint32_t pack_color(SDL_Color c) {
        return (static_cast<uint32_t>(c.r) << 16) |
               (static_cast<uint32_t>(c.g) << 8) |
               static_cast<uint32_t>(c.b);
    }

private:
    void render_glyph(CachedGlyph& glyph, uint32_t codepoint, SDL_Color color) {
        char utf8[8] = {0};
        int len = codepoint_to_utf8(codepoint, utf8);
        if (len == 0) return;

        SurfacePtr surf(TTF_RenderUTF8_Blended(font_, utf8, color));
        if (surf) {
            glyph.texture.reset(SDL_CreateTextureFromSurface(renderer_, surf.get()));
            glyph.width = surf->w;
            glyph.height = surf->h;
        }
    }

    static int codepoint_to_utf8(uint32_t cp, char* out) {
        int len = 0;
        if (cp < 0x80) {
            out[len++] = static_cast<char>(cp);
        } else if (cp < 0x800) {
            out[len++] = static_cast<char>(0xC0 | (cp >> 6));
            out[len++] = static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            out[len++] = static_cast<char>(0xE0 | (cp >> 12));
            out[len++] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out[len++] = static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp < 0x110000) {
            out[len++] = static_cast<char>(0xF0 | (cp >> 18));
            out[len++] = static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            out[len++] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out[len++] = static_cast<char>(0x80 | (cp & 0x3F));
        }
        out[len] = '\0';
        return len;
    }

    LRUCache<GlyphKey, CachedGlyph, GlyphKeyHash> cache_;
    SDL_Renderer* renderer_ = nullptr;
    TTF_Font* font_ = nullptr;
};
