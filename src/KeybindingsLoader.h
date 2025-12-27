#pragma once

#include "InputMapper.h"
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <string>
#include <string_view>
#include <charconv>

namespace KeybindingsLoader {

namespace detail {
    inline std::unordered_map<std::string, SDL_Keycode> build_keyname_map() {
        return {
            {"a", SDLK_a}, {"b", SDLK_b}, {"c", SDLK_c}, {"d", SDLK_d},
            {"e", SDLK_e}, {"f", SDLK_f}, {"g", SDLK_g}, {"h", SDLK_h},
            {"i", SDLK_i}, {"j", SDLK_j}, {"k", SDLK_k}, {"l", SDLK_l},
            {"m", SDLK_m}, {"n", SDLK_n}, {"o", SDLK_o}, {"p", SDLK_p},
            {"q", SDLK_q}, {"r", SDLK_r}, {"s", SDLK_s}, {"t", SDLK_t},
            {"u", SDLK_u}, {"v", SDLK_v}, {"w", SDLK_w}, {"x", SDLK_x},
            {"y", SDLK_y}, {"z", SDLK_z},
            {"0", SDLK_0}, {"1", SDLK_1}, {"2", SDLK_2}, {"3", SDLK_3},
            {"4", SDLK_4}, {"5", SDLK_5}, {"6", SDLK_6}, {"7", SDLK_7},
            {"8", SDLK_8}, {"9", SDLK_9},
            {"f1", SDLK_F1}, {"f2", SDLK_F2}, {"f3", SDLK_F3}, {"f4", SDLK_F4},
            {"f5", SDLK_F5}, {"f6", SDLK_F6}, {"f7", SDLK_F7}, {"f8", SDLK_F8},
            {"f9", SDLK_F9}, {"f10", SDLK_F10}, {"f11", SDLK_F11}, {"f12", SDLK_F12},
            {"enter", SDLK_RETURN}, {"return", SDLK_RETURN},
            {"escape", SDLK_ESCAPE}, {"esc", SDLK_ESCAPE},
            {"tab", SDLK_TAB},
            {"backspace", SDLK_BACKSPACE},
            {"delete", SDLK_DELETE}, {"del", SDLK_DELETE},
            {"insert", SDLK_INSERT}, {"ins", SDLK_INSERT},
            {"home", SDLK_HOME}, {"end", SDLK_END},
            {"pageup", SDLK_PAGEUP}, {"pgup", SDLK_PAGEUP},
            {"pagedown", SDLK_PAGEDOWN}, {"pgdn", SDLK_PAGEDOWN},
            {"up", SDLK_UP}, {"down", SDLK_DOWN},
            {"left", SDLK_LEFT}, {"right", SDLK_RIGHT},
            {"space", SDLK_SPACE},
            {"plus", SDLK_PLUS}, {"+", SDLK_PLUS}, {"=", SDLK_EQUALS},
            {"minus", SDLK_MINUS}, {"-", SDLK_MINUS},
            {"slash", SDLK_SLASH}, {"/", SDLK_SLASH},
            {"backslash", SDLK_BACKSLASH}, {"\\", SDLK_BACKSLASH},
            {"[", SDLK_LEFTBRACKET}, {"]", SDLK_RIGHTBRACKET},
            {"leftbracket", SDLK_LEFTBRACKET}, {"rightbracket", SDLK_RIGHTBRACKET},
            {"`", SDLK_BACKQUOTE}, {"backquote", SDLK_BACKQUOTE},
        };
    }

    inline std::string to_lower(std::string_view sv) {
        std::string result;
        result.reserve(sv.size());
        for (char c : sv) {
            result += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        return result;
    }

    inline std::vector<std::string> split(std::string_view sv, char delim) {
        std::vector<std::string> result;
        size_t start = 0;
        size_t pos = sv.find(delim);
        while (pos != std::string_view::npos) {
            result.emplace_back(sv.substr(start, pos - start));
            start = pos + 1;
            pos = sv.find(delim, start);
        }
        result.emplace_back(sv.substr(start));
        return result;
    }

    inline std::string trim(std::string_view sv) {
        size_t start = sv.find_first_not_of(" \t\r\n");
        if (start == std::string_view::npos) return "";
        size_t end = sv.find_last_not_of(" \t\r\n");
        return std::string(sv.substr(start, end - start + 1));
    }
}

inline std::optional<KeyCombo> parse_key_combo(std::string_view combo_str) {
    static const auto keyname_map = detail::build_keyname_map();

    auto parts = detail::split(combo_str, '+');
    if (parts.empty()) return std::nullopt;

    uint16_t mod = 0;
    SDL_Keycode key = SDLK_UNKNOWN;

    for (size_t i = 0; i < parts.size(); ++i) {
        std::string part = detail::to_lower(detail::trim(parts[i]));
        if (part.empty()) continue;

        if (part == "ctrl" || part == "control") {
            mod |= KMOD_CTRL;
        } else if (part == "shift") {
            mod |= KMOD_SHIFT;
        } else if (part == "alt") {
            mod |= KMOD_ALT;
        } else if (part == "meta" || part == "cmd" || part == "super" || part == "win") {
            mod |= KMOD_GUI;
        } else {
            if (auto it = keyname_map.find(part); it != keyname_map.end()) {
                key = it->second;
            } else {
                return std::nullopt;
            }
        }
    }

    if (key == SDLK_UNKNOWN) return std::nullopt;
    return KeyCombo{key, mod};
}

inline InputContext parse_context(std::string_view ctx_str) {
    std::string lower = detail::to_lower(ctx_str);
    if (lower == "editor") return InputContext::Editor;
    if (lower == "filetree" || lower == "tree") return InputContext::FileTree;
    if (lower == "terminal") return InputContext::Terminal;
    if (lower == "commandbar" || lower == "command") return InputContext::CommandBar;
    return InputContext::Global;
}

inline bool load_from_json(InputMapper& mapper, const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) return false;

    std::string line;
    std::string current_context_str = "global";
    InputContext current_context = InputContext::Global;

    while (std::getline(file, line)) {
        std::string trimmed = detail::trim(line);
        if (trimmed.empty() || trimmed[0] == '/' || trimmed[0] == '#') continue;

        if (trimmed[0] == '[' && trimmed.back() == ']') {
            current_context_str = detail::trim(trimmed.substr(1, trimmed.size() - 2));
            current_context = parse_context(current_context_str);
            continue;
        }

        size_t colon_pos = trimmed.find(':');
        if (colon_pos == std::string::npos) continue;

        std::string key_part = detail::trim(trimmed.substr(0, colon_pos));
        std::string action_part = detail::trim(trimmed.substr(colon_pos + 1));

        if (action_part.front() == '"') action_part = action_part.substr(1);
        if (action_part.back() == '"') action_part.pop_back();
        if (action_part.back() == ',') action_part.pop_back();

        if (key_part.front() == '"') key_part = key_part.substr(1);
        if (key_part.back() == '"') key_part.pop_back();

        if (auto combo = parse_key_combo(key_part)) {
            mapper.bind(*combo, action_part, current_context);
        }
    }

    return true;
}

inline void save_to_json(const InputMapper& mapper, const std::string& filepath) {
    std::ofstream file(filepath);
    if (!file.is_open()) return;

    auto write_context = [&](InputContext ctx, std::string_view name) {
        auto bindings = mapper.get_bindings(ctx);
        if (bindings.empty()) return;

        file << "[" << name << "]\n";
        for (const auto& [combo, action] : bindings) {
            file << "\"";
            if (combo.mod & KMOD_CTRL) file << "ctrl+";
            if (combo.mod & KMOD_SHIFT) file << "shift+";
            if (combo.mod & KMOD_ALT) file << "alt+";
            if (combo.mod & KMOD_GUI) file << "meta+";
            file << SDL_GetKeyName(combo.key);
            file << "\": \"" << action << "\"\n";
        }
        file << "\n";
    };

    write_context(InputContext::Global, "global");
    write_context(InputContext::Editor, "editor");
    write_context(InputContext::FileTree, "filetree");
    write_context(InputContext::Terminal, "terminal");
    write_context(InputContext::CommandBar, "commandbar");
}

}
