#pragma once

#include "Types.h"
#include "GlyphCache.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <deque>
#include <vector>
#include <cstdint>

extern "C" {
    #include <vterm.h>
}

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
    GlyphCache glyph_cache;

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
    static int sb_popline_callback(int cols, VTermScreenCell* cells, void* user);

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
