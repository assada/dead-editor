#include "TextureCache.h"

namespace {
constexpr int TAB_WIDTH = 4;

std::string expand_tabs(const std::string& text) {
    std::string result;
    result.reserve(text.size());
    int column = 0;
    for (char c : text) {
        if (c == '\t') {
            int spaces = TAB_WIDTH - (column % TAB_WIDTH);
            result.append(spaces, ' ');
            column += spaces;
        } else {
            result.push_back(c);
            column++;
        }
    }
    return result;
}

std::vector<Token> adjust_tokens_for_tabs(const std::string& original, const std::vector<Token>& tokens) {
    std::vector<int> byte_to_expanded;
    byte_to_expanded.reserve(original.size() + 1);
    int expanded_pos = 0;
    int column = 0;
    for (size_t i = 0; i < original.size(); i++) {
        byte_to_expanded.push_back(expanded_pos);
        if (original[i] == '\t') {
            int spaces = TAB_WIDTH - (column % TAB_WIDTH);
            expanded_pos += spaces;
            column += spaces;
        } else {
            expanded_pos++;
            column++;
        }
    }
    byte_to_expanded.push_back(expanded_pos);

    std::vector<Token> adjusted;
    adjusted.reserve(tokens.size());
    for (const auto& tok : tokens) {
        int new_start = (tok.start >= 0 && tok.start < static_cast<int>(byte_to_expanded.size()))
            ? byte_to_expanded[tok.start] : tok.start;
        int new_end = (tok.end >= 0 && tok.end < static_cast<int>(byte_to_expanded.size()))
            ? byte_to_expanded[tok.end] : tok.end;
        adjusted.push_back({tok.type, new_start, new_end});
    }
    return adjusted;
}
}

void CachedLineRender::reset() {
    texture.reset();
    valid = false;
    content.clear();
    tokens.clear();
    width = 0;
    height = 0;
}

bool CachedLineRender::matches(const std::string& text, const std::vector<Token>& toks) const {
    return valid && content == text && tokens == toks;
}

static SDL_Surface* render_tokenized_line(
    const std::string& line_text,
    const std::vector<Token>& tokens,
    TTF_Font* font,
    int line_height,
    SDL_Color default_color,
    const std::function<SDL_Color(TokenType)>& get_color
) {
    std::string expanded_text = expand_tabs(line_text);
    std::vector<Token> expanded_tokens = adjust_tokens_for_tabs(line_text, tokens);

    int total_width = 0;
    TTF_SizeUTF8(font, expanded_text.c_str(), &total_width, nullptr);
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

    if (expanded_tokens.empty()) {
        blit_segment(expanded_text, default_color);
    } else {
        int prev_end = 0;
        for (const auto& tok : expanded_tokens) {
            if (tok.start > prev_end) {
                blit_segment(expanded_text.substr(prev_end, tok.start - prev_end), default_color);
            }
            blit_segment(expanded_text.substr(tok.start, tok.end - tok.start), get_color(tok.type));
            prev_end = tok.end;
        }
        if (prev_end < static_cast<int>(expanded_text.size())) {
            blit_segment(expanded_text.substr(prev_end), default_color);
        }
    }

    return target;
}

void TextureCache::init(SDL_Renderer* r, TTF_Font* f) {
    renderer = r;
    font = f;
    line_height = TTF_FontHeight(f);
}

void TextureCache::invalidate_all() {
    text_cache.clear_and_trim();
    line_number_cache.clear_and_trim();
    font_version++;
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

SDL_Surface* TextureCache::render_text_to_surface(const std::string& text, SDL_Color color) {
    std::string expanded = expand_tabs(text);
    return TTF_RenderUTF8_Blended(font, expanded.c_str(), color);
}

void TextureCache::render_cached_text(const std::string& text, SDL_Color color, int x, int y) {
    if (text.empty()) return;

    uint64_t key = make_text_key(text, color);

    if (auto* cached = text_cache.get(key)) {
        SDL_Rect rect = {x, y, cached->width, cached->height};
        SDL_RenderCopy(renderer, cached->texture.get(), nullptr, &rect);
        return;
    }

    SurfacePtr surface(render_text_to_surface(text, color));
    if (!surface) return;

    CachedTexture& cached = text_cache.get_or_create(key);
    cached.texture.reset(SDL_CreateTextureFromSurface(renderer, surface.get()));
    cached.width = surface->w;
    cached.height = surface->h;

    SDL_Rect rect = {x, y, cached.width, cached.height};
    SDL_RenderCopy(renderer, cached.texture.get(), nullptr, &rect);
}

void TextureCache::render_cached_text_right_aligned(const std::string& text, SDL_Color color, int right_x, int y) {
    if (text.empty()) return;

    uint64_t key = make_text_key(text, color);

    if (auto* cached = text_cache.get(key)) {
        SDL_Rect rect = {right_x - cached->width, y, cached->width, cached->height};
        SDL_RenderCopy(renderer, cached->texture.get(), nullptr, &rect);
        return;
    }

    SurfacePtr surface(render_text_to_surface(text, color));
    if (!surface) return;

    CachedTexture& cached = text_cache.get_or_create(key);
    cached.texture.reset(SDL_CreateTextureFromSurface(renderer, surface.get()));
    cached.width = surface->w;
    cached.height = surface->h;

    SDL_Rect rect = {right_x - cached.width, y, cached.width, cached.height};
    SDL_RenderCopy(renderer, cached.texture.get(), nullptr, &rect);
}

SDL_Texture* TextureCache::get_line_number_texture(const std::string& num_str, SDL_Color color, int& w, int& h) {
    if (auto* cached = line_number_cache.get(num_str)) {
        w = cached->width;
        h = cached->height;
        return cached->texture.get();
    }

    SurfacePtr surface(render_text_to_surface(num_str, color));
    if (!surface) return nullptr;

    CachedTexture& cached = line_number_cache.get_or_create(num_str);
    cached.texture.reset(SDL_CreateTextureFromSurface(renderer, surface.get()));
    cached.width = surface->w;
    cached.height = surface->h;

    w = cached.width;
    h = cached.height;
    return cached.texture.get();
}

SDL_Surface* TextureCache::render_line_to_surface(
    const std::string& line_text,
    const std::vector<Token>& tokens,
    SDL_Color default_color,
    const std::function<SDL_Color(TokenType)>& get_color
) {
    return render_tokenized_line(line_text, tokens, font, line_height, default_color, get_color);
}

TextureCache::~TextureCache() {
    invalidate_all();
}

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
) {
    CachedLineRender& cached = cache.get_or_create(line_idx);

    if (cached.matches(line_text, tokens)) {
        return cached;
    }

    cached.reset();
    cached.content = line_text;
    cached.tokens = tokens;

    if (line_text.empty()) {
        cached.valid = true;
        cached.width = 0;
        return cached;
    }

    SurfacePtr surface(render_tokenized_line(line_text, tokens, font, line_height, default_color, get_color));
    if (surface) {
        cached.texture.reset(SDL_CreateTextureFromSurface(renderer, surface.get()));
        cached.width = surface->w;
        cached.height = surface->h;
    }
    cached.valid = true;

    return cached;
}

void render_line(const CachedLineRender& cached, SDL_Renderer* renderer, int x, int y) {
    if (cached.texture) {
        SDL_SetTextureScaleMode(cached.texture.get(), SDL_ScaleModeLinear);
        SDL_Rect rect = {x, y, cached.width, cached.height};
        SDL_RenderCopy(renderer, cached.texture.get(), nullptr, &rect);
    }
}
