#pragma once
#include <stdexcept>
#include <cstdio>
#include <cctype>
#include <algorithm>
#include <sys/stat.h>
#include <cstdint>
#include <SDL2/SDL.h>
#include "Types.h"

inline bool is_meta_pressed() {
    const Uint8* state = SDL_GetKeyboardState(nullptr);
    return state[SDL_SCANCODE_LCTRL] || state[SDL_SCANCODE_RCTRL] ||
           state[SDL_SCANCODE_LGUI] || state[SDL_SCANCODE_RGUI];
}

int safe_stoi(const std::string& str, int default_value);
std::string show_save_dialog(const std::string& default_path = "");
std::string show_open_file_dialog();
std::string show_open_folder_dialog();
FileLocation parse_file_argument(const char* arg);
void parse_goto_input(const std::string& input, LineIdx& line, ColIdx& col);
int utf8_char_len(unsigned char c);
ColIdx utf8_prev_char_start(const std::string& str, ColIdx pos);
ColIdx utf8_next_char_pos(const std::string& str, ColIdx pos);
ColIdx utf8_clamp_to_char_boundary(const std::string& str, ColIdx pos);
uint32_t utf8_decode_at(const std::string& str, ColIdx pos);
bool is_word_codepoint(uint32_t cp);
std::string expand_tabs(const std::string& text, int tab_width = 4);
int expanded_column(const std::string& text, int byte_pos, int tab_width = 4);
bool is_directory(const char* path);
std::string get_resource_path(const std::string& filename);
std::string get_config_path(const std::string& filename);
void open_containing_folder(const std::string& path);
