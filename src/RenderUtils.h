#pragma once

#include "HandleTypes.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string>

struct TextSize {
    int w = 0;
    int h = 0;
};

inline TextSize render_text(SDL_Renderer* renderer, TTF_Font* font, const std::string& text, SDL_Color color, int x, int y) {
    TextSize size;
    SurfacePtr surface(TTF_RenderUTF8_Blended(font, text.c_str(), color));
    if (surface) {
        size = {surface->w, surface->h};
        TexturePtr texture(SDL_CreateTextureFromSurface(renderer, surface.get()));
        SDL_Rect rect = {x, y, surface->w, surface->h};
        SDL_RenderCopy(renderer, texture.get(), nullptr, &rect);
    }
    return size;
}

inline TextSize get_text_size(TTF_Font* font, const std::string& text) {
    TextSize size;
    TTF_SizeUTF8(font, text.c_str(), &size.w, &size.h);
    return size;
}
