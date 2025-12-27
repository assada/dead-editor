#include "Terminal.h"
#include "HandleTypes.h"
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <cstdlib>
#include <cstring>
#include <algorithm>

#if defined(__APPLE__) || defined(__OpenBSD__) || defined(__NetBSD__)
#include <util.h>
#else
#include <pty.h>
#endif

void TerminalGlyphCache::init(SDL_Renderer* r, TTF_Font* f) {
    renderer = r;
    font = f;
}

void TerminalGlyphCache::clear() {
    for (auto& [key, glyph] : cache) {
        if (glyph.texture) {
            SDL_DestroyTexture(glyph.texture);
        }
    }
    cache.clear();
}

TerminalGlyphCache::~TerminalGlyphCache() {
    clear();
}

uint32_t TerminalGlyphCache::pack_color(SDL_Color c) {
    return (static_cast<uint32_t>(c.r) << 16) | (static_cast<uint32_t>(c.g) << 8) | static_cast<uint32_t>(c.b);
}

TerminalGlyphCache::CachedGlyph* TerminalGlyphCache::get_or_create(uint32_t codepoint, SDL_Color fg, bool bold) {
    GlyphKey key{codepoint, pack_color(fg), bold};

    auto it = cache.find(key);
    if (it != cache.end()) {
        return &it->second;
    }

    if (cache.size() >= max_cache_size) {
        auto oldest = cache.begin();
        if (oldest->second.texture) {
            SDL_DestroyTexture(oldest->second.texture);
        }
        cache.erase(oldest);
    }

    char utf8[8] = {0};
    int len = 0;
    if (codepoint < 0x80) {
        utf8[len++] = static_cast<char>(codepoint);
    } else if (codepoint < 0x800) {
        utf8[len++] = static_cast<char>(0xC0 | (codepoint >> 6));
        utf8[len++] = static_cast<char>(0x80 | (codepoint & 0x3F));
    } else if (codepoint < 0x10000) {
        utf8[len++] = static_cast<char>(0xE0 | (codepoint >> 12));
        utf8[len++] = static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
        utf8[len++] = static_cast<char>(0x80 | (codepoint & 0x3F));
    } else {
        utf8[len++] = static_cast<char>(0xF0 | (codepoint >> 18));
        utf8[len++] = static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
        utf8[len++] = static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
        utf8[len++] = static_cast<char>(0x80 | (codepoint & 0x3F));
    }
    utf8[len] = '\0';

    CachedGlyph glyph;
    SurfacePtr surf(TTF_RenderUTF8_Blended(font, utf8, fg));
    if (surf) {
        glyph.texture = SDL_CreateTextureFromSurface(renderer, surf.get());
        glyph.width = surf->w;
        glyph.height = surf->h;
    }

    auto [inserted_it, success] = cache.emplace(key, glyph);
    return &inserted_it->second;
}

TerminalEmulator::~TerminalEmulator() {
    destroy();
}

void TerminalEmulator::destroy() {
    glyph_cache.clear();
    scrollback_buffer.clear();
    scroll_offset = 0;
    if (master_fd != -1) {
        close(master_fd);
        master_fd = -1;
    }
    if (child_pid > 0) {
        kill(child_pid, SIGTERM);
        waitpid(child_pid, nullptr, WNOHANG);
        child_pid = -1;
    }
    if (vterm) {
        vterm_free(vterm);
        vterm = nullptr;
        screen = nullptr;
    }
}

int TerminalEmulator::damage_callback(VTermRect rect, void* user) {
    auto* self = static_cast<TerminalEmulator*>(user);
    (void)rect;
    self->needs_redraw = true;
    return 0;
}

int TerminalEmulator::movecursor_callback(VTermPos pos, VTermPos oldpos, int visible, void* user) {
    auto* self = static_cast<TerminalEmulator*>(user);
    (void)pos; (void)oldpos; (void)visible;
    self->needs_redraw = true;
    return 0;
}

int TerminalEmulator::bell_callback(void* user) {
    (void)user;
    return 0;
}

int TerminalEmulator::sb_pushline_callback(int cols, const VTermScreenCell* cells, void* user) {
    auto* self = static_cast<TerminalEmulator*>(user);

    std::vector<ScrollbackCell> line;
    line.reserve(static_cast<size_t>(cols));

    for (int i = 0; i < cols; i++) {
        ScrollbackCell sc;
        sc.codepoint = cells[i].chars[0];
        sc.fg = self->vterm_color_to_sdl(cells[i].fg);
        sc.bg = self->vterm_color_to_sdl(cells[i].bg);
        sc.width = static_cast<uint8_t>(cells[i].width);
        sc.bold = cells[i].attrs.bold;
        sc.reverse = cells[i].attrs.reverse;
        line.push_back(sc);
    }

    self->scrollback_buffer.push_back(std::move(line));

    if (self->scrollback_buffer.size() > MAX_SCROLLBACK) {
        self->scrollback_buffer.pop_front();
    }

    return 1;
}

void TerminalEmulator::spawn(int width, int height, int fw, int fh, FocusPanel* focus_ptr, SDL_Renderer* renderer, TTF_Font* font) {
    font_width = fw;
    font_height = fh;
    current_focus = focus_ptr;
    term_cols = std::max(10, width / std::max(1, fw));
    term_rows = std::max(2, height / std::max(1, fh));

    glyph_cache.init(renderer, font);
    scrollback_buffer.clear();
    scroll_offset = 0;

    vterm = vterm_new(term_rows, term_cols);
    vterm_set_utf8(vterm, 1);
    screen = vterm_obtain_screen(vterm);

    VTermState* state = vterm_obtain_state(vterm);
    VTermColor fg_vterm, bg_vterm;
    vterm_color_rgb(&fg_vterm, default_fg.r, default_fg.g, default_fg.b);
    vterm_color_rgb(&bg_vterm, default_bg.r, default_bg.g, default_bg.b);
    vterm_state_set_default_colors(state, &fg_vterm, &bg_vterm);

    static VTermScreenCallbacks screen_callbacks = {};
    screen_callbacks.damage = damage_callback;
    screen_callbacks.movecursor = movecursor_callback;
    screen_callbacks.bell = bell_callback;
    screen_callbacks.sb_pushline = sb_pushline_callback;
    vterm_screen_set_callbacks(screen, &screen_callbacks, this);

    vterm_screen_enable_altscreen(screen, 1);
    vterm_screen_reset(screen, 1);

    struct winsize win = {
        static_cast<unsigned short>(term_rows),
        static_cast<unsigned short>(term_cols),
        static_cast<unsigned short>(width),
        static_cast<unsigned short>(height)
    };

    child_pid = forkpty(&master_fd, nullptr, nullptr, &win);

    if (child_pid == 0) {
        setenv("TERM", "xterm-256color", 1);
        setenv("COLORTERM", "truecolor", 1);
        const char* shell = getenv("SHELL");
        if (!shell) shell = "/bin/bash";
        execlp(shell, shell, nullptr);
        _exit(1);
    } else if (child_pid > 0) {
        int flags = fcntl(master_fd, F_GETFL, 0);
        fcntl(master_fd, F_SETFL, flags | O_NONBLOCK);
    }
}

void TerminalEmulator::resize(int width, int height) {
    if (master_fd == -1 || !vterm) return;
    int new_cols = std::max(10, width / std::max(1, font_width));
    int new_rows = std::max(2, height / std::max(1, font_height));

    if (new_cols != term_cols || new_rows != term_rows) {
        term_cols = new_cols;
        term_rows = new_rows;

        vterm_set_size(vterm, term_rows, term_cols);
        vterm_screen_flush_damage(screen);

        struct winsize win = {
            static_cast<unsigned short>(term_rows),
            static_cast<unsigned short>(term_cols),
            static_cast<unsigned short>(width),
            static_cast<unsigned short>(height)
        };
        ioctl(master_fd, TIOCSWINSZ, &win);
        needs_redraw = true;
    }
}

void TerminalEmulator::write_input(const char* data, size_t len) {
    if (master_fd != -1 && len > 0) {
        scroll_offset = 0;
        write(master_fd, data, len);
    }
}

void TerminalEmulator::write_input(const std::string& data) {
    write_input(data.c_str(), data.size());
}

void TerminalEmulator::update() {
    if (master_fd == -1) return;

    int status;
    pid_t result = waitpid(child_pid, &status, WNOHANG);
    if (result > 0) {
        destroy();
        return;
    }

    char buf[4096];
    ssize_t n;
    while ((n = read(master_fd, buf, sizeof(buf))) > 0) {
        vterm_input_write(vterm, buf, static_cast<size_t>(n));
    }
}

SDL_Color TerminalEmulator::vterm_color_to_sdl(const VTermColor& color) {
    if (VTERM_COLOR_IS_DEFAULT_FG(&color)) {
        return default_fg;
    }
    if (VTERM_COLOR_IS_DEFAULT_BG(&color)) {
        return default_bg;
    }
    if (VTERM_COLOR_IS_INDEXED(&color)) {
        int idx = color.indexed.idx;
        if (idx < 16) {
            return PALETTE_16[idx];
        }
        VTermColor rgb_color = color;
        vterm_screen_convert_color_to_rgb(screen, &rgb_color);
        return {rgb_color.rgb.red, rgb_color.rgb.green, rgb_color.rgb.blue, 255};
    }
    return {color.rgb.red, color.rgb.green, color.rgb.blue, 255};
}

void TerminalEmulator::handle_mouse_wheel(int wheel_y) {
    if (master_fd == -1 || !vterm) return;

    if (scrollback_buffer.empty() && scroll_offset == 0) {
        const char* code = (wheel_y > 0) ? "\x1b[A" : "\x1b[B";
        for (int i = 0; i < 3; i++) {
            ::write(master_fd, code, 3);
        }
    } else {
        scroll_offset += wheel_y * 3;

        if (scroll_offset < 0) scroll_offset = 0;
        int max_scroll = static_cast<int>(scrollback_buffer.size());
        if (scroll_offset > max_scroll) scroll_offset = max_scroll;

        needs_redraw = true;
    }
}

void TerminalEmulator::render(SDL_Renderer* renderer, TTF_Font* font, int x, int y, int width, int height) {
    if (!screen) return;

    (void)font;

    SDL_Rect term_rect = {x, y, width, height};
    SDL_RenderSetClipRect(renderer, &term_rect);

    VTermState* state = vterm_obtain_state(vterm);
    VTermPos cursor_pos;
    vterm_state_get_cursorpos(state, &cursor_pos);

    int total_history = static_cast<int>(scrollback_buffer.size());

    for (int row = 0; row < term_rows && (row * font_height) < height; row++) {
        int draw_y = y + row * font_height;
        int draw_x = x;

        int history_idx = -1;
        int vterm_row = -1;

        if (scroll_offset > 0) {
            int history_start = total_history - scroll_offset;
            int target_idx = history_start + row;

            if (target_idx < 0) {
                continue;
            } else if (target_idx < total_history) {
                history_idx = target_idx;
            } else {
                vterm_row = target_idx - total_history;
            }
        } else {
            vterm_row = row;
        }

        if (vterm_row >= term_rows) continue;

        for (int col = 0; col < term_cols && draw_x < x + width; col++) {
            SDL_Color bg, fg;
            uint32_t codepoint = 0;
            int cell_width = 1;
            bool bold = false;
            bool reverse = false;

            if (history_idx != -1) {
                const auto& line = scrollback_buffer[history_idx];
                if (col < static_cast<int>(line.size())) {
                    const auto& cell = line[col];
                    codepoint = cell.codepoint;
                    fg = cell.fg;
                    bg = cell.bg;
                    cell_width = cell.width;
                    bold = cell.bold;
                    reverse = cell.reverse;
                } else {
                    codepoint = 0;
                    fg = default_fg;
                    bg = default_bg;
                }
            } else {
                VTermScreenCell cell;
                vterm_screen_get_cell(screen, {vterm_row, col}, &cell);
                codepoint = cell.chars[0];
                fg = vterm_color_to_sdl(cell.fg);
                bg = vterm_color_to_sdl(cell.bg);
                cell_width = cell.width;
                bold = cell.attrs.bold;
                reverse = cell.attrs.reverse;
            }

            if (reverse) {
                std::swap(bg, fg);
            }

            bool is_cursor = (scroll_offset == 0 && vterm_row == cursor_pos.row && col == cursor_pos.col &&
                              current_focus && *current_focus == FocusPanel::Terminal);

            if (is_cursor) {
                bg = {200, 200, 200, 255};
                fg = {25, 25, 30, 255};
            }

            bool bg_not_default = (bg.r != default_bg.r || bg.g != default_bg.g || bg.b != default_bg.b);
            if (bg_not_default || is_cursor) {
                SDL_SetRenderDrawColor(renderer, bg.r, bg.g, bg.b, 255);
                SDL_Rect cell_rect = {draw_x, draw_y, font_width * cell_width, font_height};
                SDL_RenderFillRect(renderer, &cell_rect);
            }

            if (codepoint != 0 && codepoint != ' ') {
                SDL_Color render_fg = fg;
                if (bold) {
                    render_fg.r = static_cast<uint8_t>(std::min(255, render_fg.r + 50));
                    render_fg.g = static_cast<uint8_t>(std::min(255, render_fg.g + 50));
                    render_fg.b = static_cast<uint8_t>(std::min(255, render_fg.b + 50));
                }

                auto* glyph = glyph_cache.get_or_create(codepoint, render_fg, bold);
                if (glyph && glyph->texture) {
                    SDL_Rect dst = {draw_x, draw_y, glyph->width, glyph->height};
                    SDL_RenderCopy(renderer, glyph->texture, nullptr, &dst);
                }
            }

            draw_x += font_width * cell_width;
            if (cell_width > 1) {
                col += cell_width - 1;
            }
        }
    }

    if (scroll_offset > 0) {
        char scroll_buf[32];
        snprintf(scroll_buf, sizeof(scroll_buf), "[%d/%d]", scroll_offset, total_history);
        SDL_Color info_color = {150, 150, 150, 255};
        int info_x = x + width - static_cast<int>(strlen(scroll_buf)) * font_width - 5;
        int info_y = y + 2;
        for (const char* p = scroll_buf; *p; ++p) {
            char c = *p;
            auto* g = glyph_cache.get_or_create(static_cast<uint32_t>(c), info_color, false);
            if (g && g->texture) {
                SDL_Rect dst = {info_x, info_y, g->width, g->height};
                SDL_RenderCopy(renderer, g->texture, nullptr, &dst);
                info_x += font_width;
            }
        }
    }

    SDL_RenderSetClipRect(renderer, nullptr);
    needs_redraw = false;
}

void TerminalEmulator::handle_key_event(const SDL_Event& event) {
    if (!is_running()) return;

    VTermModifier mod = VTERM_MOD_NONE;
    if (event.key.keysym.mod & KMOD_CTRL) mod = static_cast<VTermModifier>(mod | VTERM_MOD_CTRL);
    if (event.key.keysym.mod & KMOD_SHIFT) mod = static_cast<VTermModifier>(mod | VTERM_MOD_SHIFT);
    if (event.key.keysym.mod & KMOD_ALT) mod = static_cast<VTermModifier>(mod | VTERM_MOD_ALT);

    VTermKey vkey = VTERM_KEY_NONE;

    switch (event.key.keysym.sym) {
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            vkey = VTERM_KEY_ENTER;
            break;
        case SDLK_BACKSPACE:
            vkey = VTERM_KEY_BACKSPACE;
            break;
        case SDLK_TAB:
            vkey = VTERM_KEY_TAB;
            break;
        case SDLK_ESCAPE:
            vkey = VTERM_KEY_ESCAPE;
            break;
        case SDLK_UP:
            vkey = VTERM_KEY_UP;
            break;
        case SDLK_DOWN:
            vkey = VTERM_KEY_DOWN;
            break;
        case SDLK_RIGHT:
            vkey = VTERM_KEY_RIGHT;
            break;
        case SDLK_LEFT:
            vkey = VTERM_KEY_LEFT;
            break;
        case SDLK_HOME:
            vkey = VTERM_KEY_HOME;
            break;
        case SDLK_END:
            vkey = VTERM_KEY_END;
            break;
        case SDLK_DELETE:
            vkey = VTERM_KEY_DEL;
            break;
        case SDLK_PAGEUP:
            vkey = VTERM_KEY_PAGEUP;
            break;
        case SDLK_PAGEDOWN:
            vkey = VTERM_KEY_PAGEDOWN;
            break;
        case SDLK_INSERT:
            vkey = VTERM_KEY_INS;
            break;
        case SDLK_F1: vkey = static_cast<VTermKey>(VTERM_KEY_FUNCTION(1)); break;
        case SDLK_F2: vkey = static_cast<VTermKey>(VTERM_KEY_FUNCTION(2)); break;
        case SDLK_F3: vkey = static_cast<VTermKey>(VTERM_KEY_FUNCTION(3)); break;
        case SDLK_F4: vkey = static_cast<VTermKey>(VTERM_KEY_FUNCTION(4)); break;
        case SDLK_F5: vkey = static_cast<VTermKey>(VTERM_KEY_FUNCTION(5)); break;
        case SDLK_F6: vkey = static_cast<VTermKey>(VTERM_KEY_FUNCTION(6)); break;
        case SDLK_F7: vkey = static_cast<VTermKey>(VTERM_KEY_FUNCTION(7)); break;
        case SDLK_F8: vkey = static_cast<VTermKey>(VTERM_KEY_FUNCTION(8)); break;
        case SDLK_F9: vkey = static_cast<VTermKey>(VTERM_KEY_FUNCTION(9)); break;
        case SDLK_F10: vkey = static_cast<VTermKey>(VTERM_KEY_FUNCTION(10)); break;
        case SDLK_F11: vkey = static_cast<VTermKey>(VTERM_KEY_FUNCTION(11)); break;
        case SDLK_F12: vkey = static_cast<VTermKey>(VTERM_KEY_FUNCTION(12)); break;
        default:
            break;
    }

    if (vkey != VTERM_KEY_NONE) {
        vterm_keyboard_key(vterm, vkey, mod);
        flush_output();
        return;
    }

    if (mod & VTERM_MOD_CTRL) {
        char c = static_cast<char>(event.key.keysym.sym);
        if (c >= 'a' && c <= 'z') {
            char ctrl_char = static_cast<char>(c - 'a' + 1);
            write_input(&ctrl_char, 1);
        }
    }
}

void TerminalEmulator::flush_output() {
    if (!vterm || master_fd == -1) return;

    size_t len = vterm_output_get_buffer_current(vterm);
    if (len > 0) {
        char buf[512];
        while (len > 0) {
            size_t chunk = std::min(len, sizeof(buf));
            size_t read = vterm_output_read(vterm, buf, chunk);
            if (read > 0) {
                ::write(master_fd, buf, read);
            }
            len -= read;
        }
    }
}

bool TerminalEmulator::is_running() const {
    return master_fd != -1 && child_pid > 0;
}
