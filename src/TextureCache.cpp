#include "TextureCache.h"
#include "HandleTypes.h"

void CachedLineRender::destroy() {
    if (texture) {
        SDL_DestroyTexture(texture);
        texture = nullptr;
    }
    valid = false;
    content.clear();
    tokens.clear();
    width = 0;
    height = 0;
}

void CachedTexture::destroy() {
    if (texture) {
        SDL_DestroyTexture(texture);
        texture = nullptr;
    }
    width = 0;
    height = 0;
    content.clear();
}

void TextureCache::init(SDL_Renderer* r, TTF_Font* f) {
    renderer = r;
    font = f;
    line_height = TTF_FontHeight(f);
}

void TextureCache::evict_lru() {
    while (line_renders.size() > MAX_CACHED_LINES && !lru_order.empty()) {
        size_t oldest = lru_order.back();
        lru_order.pop_back();
        lru_map.erase(oldest);
        auto it = line_renders.find(oldest);
        if (it != line_renders.end()) {
            it->second.destroy();
            line_renders.erase(it);
        }
    }
}

void TextureCache::touch_line(size_t line_idx) {
    auto it = lru_map.find(line_idx);
    if (it != lru_map.end()) {
        lru_order.erase(it->second);
    }
    lru_order.push_front(line_idx);
    lru_map[line_idx] = lru_order.begin();
}

void TextureCache::invalidate_all() {
    for (auto& pair : line_renders) {
        pair.second.destroy();
    }
    line_renders.clear();
    lru_order.clear();
    lru_map.clear();
    for (auto& pair : line_number_textures) {
        pair.second.destroy();
    }
    line_number_textures.clear();
    for (auto& pair : text_cache) {
        pair.second.destroy();
    }
    text_cache.clear();
    font_version++;
}

void TextureCache::invalidate_line(size_t line_idx) {
    auto it = line_renders.find(line_idx);
    if (it != line_renders.end()) {
        it->second.destroy();
        line_renders.erase(it);
    }
    auto lru_it = lru_map.find(line_idx);
    if (lru_it != lru_map.end()) {
        lru_order.erase(lru_it->second);
        lru_map.erase(lru_it);
    }
}

void TextureCache::set_font(TTF_Font* f) {
    if (font != f) {
        invalidate_all();
        font = f;
        line_height = TTF_FontHeight(f);
    }
}

uint64_t TextureCache::make_text_key(const std::string& text, SDL_Color color) {
    uint64_t hash = 14695981039346656037ULL;
    for (char c : text) {
        hash ^= static_cast<uint64_t>(static_cast<unsigned char>(c));
        hash *= 1099511628211ULL;
    }
    hash ^= (static_cast<uint64_t>(color.r) << 24) |
            (static_cast<uint64_t>(color.g) << 16) |
            (static_cast<uint64_t>(color.b) << 8) |
            static_cast<uint64_t>(color.a);
    hash *= 1099511628211ULL;
    return hash;
}

void TextureCache::render_cached_text(const std::string& text, SDL_Color color, int x, int y) {
    if (text.empty()) return;

    uint64_t key = make_text_key(text, color);
    auto it = text_cache.find(key);

    if (it != text_cache.end() && it->second.texture) {
        SDL_Rect rect = {x, y, it->second.width, it->second.height};
        SDL_RenderCopy(renderer, it->second.texture, nullptr, &rect);
        return;
    }

    if (text_cache.size() >= MAX_CACHED_TEXT) {
        auto oldest = text_cache.begin();
        oldest->second.destroy();
        text_cache.erase(oldest);
    }

    SurfacePtr surface(TTF_RenderUTF8_Blended(font, text.c_str(), color));
    if (!surface) return;

    CachedTexture cached;
    cached.texture = SDL_CreateTextureFromSurface(renderer, surface.get());
    cached.width = surface->w;
    cached.height = surface->h;

    SDL_Rect rect = {x, y, cached.width, cached.height};
    SDL_RenderCopy(renderer, cached.texture, nullptr, &rect);

    text_cache[key] = cached;
}

void TextureCache::render_cached_text_right_aligned(const std::string& text, SDL_Color color, int right_x, int y) {
    if (text.empty()) return;

    uint64_t key = make_text_key(text, color);
    auto it = text_cache.find(key);

    if (it != text_cache.end() && it->second.texture) {
        SDL_Rect rect = {right_x - it->second.width, y, it->second.width, it->second.height};
        SDL_RenderCopy(renderer, it->second.texture, nullptr, &rect);
        return;
    }

    if (text_cache.size() >= MAX_CACHED_TEXT) {
        auto oldest = text_cache.begin();
        oldest->second.destroy();
        text_cache.erase(oldest);
    }

    SurfacePtr surface(TTF_RenderUTF8_Blended(font, text.c_str(), color));
    if (!surface) return;

    CachedTexture cached;
    cached.texture = SDL_CreateTextureFromSurface(renderer, surface.get());
    cached.width = surface->w;
    cached.height = surface->h;

    SDL_Rect rect = {right_x - cached.width, y, cached.width, cached.height};
    SDL_RenderCopy(renderer, cached.texture, nullptr, &rect);

    text_cache[key] = cached;
}

SDL_Texture* TextureCache::get_line_number_texture(const std::string& num_str, SDL_Color color, int& w, int& h) {
    auto it = line_number_textures.find(num_str);
    if (it != line_number_textures.end() && it->second.texture) {
        w = it->second.width;
        h = it->second.height;
        return it->second.texture;
    }

    SurfacePtr surface(TTF_RenderUTF8_Blended(font, num_str.c_str(), color));
    if (!surface) return nullptr;

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface.get());
    CachedTexture cached;
    cached.texture = texture;
    cached.width = surface->w;
    cached.height = surface->h;
    cached.content = num_str;

    w = cached.width;
    h = cached.height;
    line_number_textures[num_str] = cached;
    return texture;
}

SDL_Surface* TextureCache::render_line_to_surface(
    const std::string& line_text,
    const std::vector<Token>& tokens,
    SDL_Color default_color,
    const std::function<SDL_Color(TokenType)>& get_color
) {
    int total_width = 0;
    TTF_SizeUTF8(font, line_text.c_str(), &total_width, nullptr);
    if (total_width <= 0) return nullptr;

    SDL_Surface* target = SDL_CreateRGBSurfaceWithFormat(
        0, total_width, line_height, 32, SDL_PIXELFORMAT_ARGB8888
    );
    if (!target) return nullptr;

    SDL_SetSurfaceBlendMode(target, SDL_BLENDMODE_BLEND);
    SDL_FillRect(target, nullptr, SDL_MapRGBA(target->format, 0, 0, 0, 0));

    int current_x = 0;

    auto blit_segment = [&](const std::string& text, SDL_Color color) {
        if (text.empty()) return;
        SurfacePtr seg(TTF_RenderUTF8_Blended(font, text.c_str(), color));
        if (seg) {
            SDL_Rect dst = {current_x, 0, seg->w, seg->h};
            SDL_SetSurfaceBlendMode(seg.get(), SDL_BLENDMODE_NONE);
            SDL_BlitSurface(seg.get(), nullptr, target, &dst);
            current_x += seg->w;
        }
    };

    if (tokens.empty()) {
        blit_segment(line_text, default_color);
    } else {
        int prev_end = 0;
        for (const auto& tok : tokens) {
            if (tok.start > prev_end) {
                blit_segment(line_text.substr(prev_end, tok.start - prev_end), default_color);
            }
            blit_segment(line_text.substr(tok.start, tok.end - tok.start), get_color(tok.type));
            prev_end = tok.end;
        }
        if (prev_end < static_cast<int>(line_text.size())) {
            blit_segment(line_text.substr(prev_end), default_color);
        }
    }

    return target;
}

CachedLineRender& TextureCache::get_or_build_line_render(
    size_t line_idx,
    const std::string& line_text,
    const std::vector<Token>& tokens,
    SDL_Color default_color,
    const std::function<SDL_Color(TokenType)>& get_color
) {
    touch_line(line_idx);
    CachedLineRender& cached = line_renders[line_idx];

    if (cached.valid && cached.content == line_text && cached.tokens == tokens) {
        return cached;
    }

    cached.destroy();
    cached.content = line_text;
    cached.tokens = tokens;

    if (line_text.empty()) {
        cached.valid = true;
        cached.width = 0;
        return cached;
    }

    SurfacePtr surface(render_line_to_surface(line_text, tokens, default_color, get_color));
    if (surface) {
        cached.texture = SDL_CreateTextureFromSurface(renderer, surface.get());
        cached.width = surface->w;
        cached.height = surface->h;
    }

    cached.valid = true;
    evict_lru();
    return cached;
}

void TextureCache::render_cached_line(const CachedLineRender& cached, int x, int y) {
    if (cached.texture) {
        SDL_SetTextureScaleMode(cached.texture, SDL_ScaleModeLinear);
        SDL_Rect rect = {x, y, cached.width, cached.height};
        SDL_RenderCopy(renderer, cached.texture, nullptr, &rect);
    }
}

TextureCache::~TextureCache() {
    invalidate_all();
}
