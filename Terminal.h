#pragma once

#include "Types.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <deque>
#include <vector>
#include <unordered_map>
#include <cstdint>

extern "C" {
    #include <vterm.h>
}

struct TerminalGlyphCache {
    struct GlyphKey {
        uint32_t codepoint;
        uint32_t color_packed;
        bool bold;

        bool operator==(const GlyphKey& other) const {
            return codepoint == other.codepoint && color_packed == other.color_packed && bold == other.bold;
        }
    };

    struct GlyphKeyHash {
        size_t operator()(const GlyphKey& k) const {
            return std::hash<uint64_t>()(
                (static_cast<uint64_t>(k.codepoint) << 32) |
                (static_cast<uint64_t>(k.color_packed) << 1) |
                static_cast<uint64_t>(k.bold)
            );
        }
    };

    struct CachedGlyph {
        SDL_Texture* texture = nullptr;
        int width = 0;
        int height = 0;
    };

    std::unordered_map<GlyphKey, CachedGlyph, GlyphKeyHash> cache;
    SDL_Renderer* renderer = nullptr;
    TTF_Font* font = nullptr;
    size_t max_cache_size = 4096;

    void init(SDL_Renderer* r, TTF_Font* f);
    void clear();
    ~TerminalGlyphCache();

    static uint32_t pack_color(SDL_Color c);
    CachedGlyph* get_or_create(uint32_t codepoint, SDL_Color fg, bool bold);
};

struct ScrollbackCell {
    uint32_t codepoint = 0;
    SDL_Color fg = {220, 220, 220, 255};
    SDL_Color bg = {18, 18, 22, 255};
    uint8_t width = 1;
    bool bold = false;
    bool reverse = false;
};

struct TerminalEmulator {
    int master_fd = -1;
    pid_t child_pid = -1;
    VTerm* vterm = nullptr;
    VTermScreen* screen = nullptr;
    int term_cols = 80;
    int term_rows = 24;
    int font_width = 0;
    int font_height = 0;
    bool needs_redraw = true;
    FocusPanel* current_focus = nullptr;
    TerminalGlyphCache glyph_cache;

    std::deque<std::vector<ScrollbackCell>> scrollback_buffer;
    int scroll_offset = 0;
    static constexpr size_t MAX_SCROLLBACK = 5000;

    static constexpr SDL_Color PALETTE_16[16] = {
        {0, 0, 0, 255},
        {205, 49, 49, 255},
        {13, 188, 121, 255},
        {229, 229, 16, 255},
        {36, 114, 200, 255},
        {188, 63, 188, 255},
        {17, 168, 205, 255},
        {229, 229, 229, 255},
        {102, 102, 102, 255},
        {241, 76, 76, 255},
        {35, 209, 139, 255},
        {245, 245, 67, 255},
        {59, 142, 234, 255},
        {214, 112, 214, 255},
        {41, 184, 219, 255},
        {255, 255, 255, 255}
    };

    SDL_Color default_fg = {220, 220, 220, 255};
    SDL_Color default_bg = {18, 18, 22, 255};

    ~TerminalEmulator();

    void destroy();

    static int damage_callback(VTermRect rect, void* user);
    static int movecursor_callback(VTermPos pos, VTermPos oldpos, int visible, void* user);
    static int bell_callback(void* user);
    static int sb_pushline_callback(int cols, const VTermScreenCell* cells, void* user);

    void spawn(int width, int height, int fw, int fh, FocusPanel* focus_ptr, SDL_Renderer* renderer, TTF_Font* font);
    void resize(int width, int height);
    void write_input(const char* data, size_t len);
    void write_input(const std::string& data);
    void update();
    SDL_Color vterm_color_to_sdl(const VTermColor& color);
    void handle_mouse_wheel(int wheel_y);
    void handle_key_event(const SDL_Event& event);
    void flush_output();
    void render(SDL_Renderer* renderer, TTF_Font* font, int x, int y, int width, int height);
    bool is_running() const;
};
