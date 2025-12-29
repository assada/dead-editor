#include "Utils.h"
#include "HandleTypes.h"
#include <stdexcept>
#include <cctype>
#include <algorithm>
#include <sys/stat.h>
#include <filesystem>
#include <SDL2/SDL.h>
#include "Types.h"

int safe_stoi(const std::string& str, int default_value = 0) {
    if (str.empty()) return default_value;
    try {
        return std::stoi(str);
    } catch (const std::invalid_argument&) {
        return default_value;
    } catch (const std::out_of_range&) {
        return default_value;
    }
}

namespace {

std::string run_dialog_command(const std::string& command) {
    PipeHandle pipe(popen(command.c_str(), "r"));
    if (!pipe) return "";
    char buffer[4096];
    std::string result;
    while (fgets(buffer, sizeof(buffer), pipe.get())) {
        result += buffer;
    }
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) {
        result.pop_back();
    }
    return result;
}

bool command_exists(const std::string& cmd) {
    std::string check = "command -v " + cmd + " >/dev/null 2>&1";
    return system(check.c_str()) == 0;
}

}

std::string show_save_dialog(const std::string& default_path) {
#ifdef __APPLE__
    std::string script = "osascript -e 'POSIX path of (choose file name";
    if (!default_path.empty()) {
        script += " default name \"" + std::filesystem::path(default_path).filename().string() + "\"";
    }
    script += ")' 2>/dev/null";
    return run_dialog_command(script);
#else
    std::string cmd;
    if (command_exists("zenity")) {
        cmd = "zenity --file-selection --save --confirm-overwrite";
        if (!default_path.empty()) {
            cmd += " --filename=\"" + default_path + "\"";
        }
    } else if (command_exists("kdialog")) {
        cmd = "kdialog --getsavefilename";
        if (!default_path.empty()) {
            cmd += " \"" + default_path + "\"";
        } else {
            cmd += " .";
        }
    } else {
        return "";
    }
    cmd += " 2>/dev/null";
    return run_dialog_command(cmd);
#endif
}

std::string show_open_file_dialog() {
#ifdef __APPLE__
    return run_dialog_command("osascript -e 'POSIX path of (choose file)' 2>/dev/null");
#else
    std::string cmd;
    if (command_exists("zenity")) {
        cmd = "zenity --file-selection 2>/dev/null";
    } else if (command_exists("kdialog")) {
        cmd = "kdialog --getopenfilename . 2>/dev/null";
    } else {
        return "";
    }
    return run_dialog_command(cmd);
#endif
}

std::string show_open_folder_dialog() {
#ifdef __APPLE__
    return run_dialog_command("osascript -e 'POSIX path of (choose folder)' 2>/dev/null");
#else
    std::string cmd;
    if (command_exists("zenity")) {
        cmd = "zenity --file-selection --directory 2>/dev/null";
    } else if (command_exists("kdialog")) {
        cmd = "kdialog --getexistingdirectory . 2>/dev/null";
    } else {
        return "";
    }
    return run_dialog_command(cmd);
#endif
}

FileLocation parse_file_argument(const char* arg) {
    FileLocation loc;
    std::string str(arg);
    size_t last_colon = str.rfind(':');
    if (last_colon != std::string::npos && last_colon > 0) {
        size_t second_last = str.rfind(':', last_colon - 1);
        if (second_last != std::string::npos && second_last > 0) {
            loc.path = str.substr(0, second_last);
            loc.pos.line = safe_stoi(str.substr(second_last + 1, last_colon - second_last - 1));
            loc.pos.col = safe_stoi(str.substr(last_colon + 1));
        } else {
            bool is_number = true;
            for (size_t i = last_colon + 1; i < str.size() && is_number; i++) {
                if (!std::isdigit(str[i])) is_number = false;
            }
            if (is_number && last_colon + 1 < str.size()) {
                loc.path = str.substr(0, last_colon);
                loc.pos.line = safe_stoi(str.substr(last_colon + 1));
            } else {
                loc.path = str;
            }
        }
    } else {
        loc.path = str;
    }
    return loc;
}

void parse_goto_input(const std::string& input, LineIdx& line, ColIdx& col) {
    line = 0;
    col = 0;
    size_t colon = input.find(':');
    if (colon != std::string::npos) {
        line = safe_stoi(input.substr(0, colon));
        col = safe_stoi(input.substr(colon + 1));
    } else {
        line = safe_stoi(input);
    }
}

int utf8_char_len(unsigned char c) {
    if ((c & 0x80) == 0) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1;
}

ColIdx utf8_prev_char_start(const std::string& str, ColIdx pos) {
    if (pos <= 0) return 0;
    pos--;
    while (pos > 0 && (str[pos] & 0xC0) == 0x80) {
        pos--;
    }
    return pos;
}

ColIdx utf8_next_char_pos(const std::string& str, ColIdx pos) {
    if (pos >= static_cast<ColIdx>(str.size())) return static_cast<ColIdx>(str.size());
    int len = utf8_char_len(static_cast<unsigned char>(str[pos]));
    return std::min(pos + len, static_cast<ColIdx>(str.size()));
}

ColIdx utf8_clamp_to_char_boundary(const std::string& str, ColIdx pos) {
    int len = static_cast<int>(str.size());
    if (pos >= len) return len;
    while (pos > 0 && (str[pos] & 0xC0) == 0x80) {
        pos--;
    }
    return pos;
}

uint32_t utf8_decode_at(const std::string& str, ColIdx pos) {
    if (pos < 0 || pos >= static_cast<ColIdx>(str.size())) return 0;
    unsigned char c = static_cast<unsigned char>(str[pos]);

    if ((c & 0x80) == 0) {
        return c;
    }
    if ((c & 0xE0) == 0xC0 && pos + 1 < static_cast<int>(str.size())) {
        return ((c & 0x1F) << 6) | (str[pos + 1] & 0x3F);
    }
    if ((c & 0xF0) == 0xE0 && pos + 2 < static_cast<int>(str.size())) {
        return ((c & 0x0F) << 12) | ((str[pos + 1] & 0x3F) << 6) | (str[pos + 2] & 0x3F);
    }
    if ((c & 0xF8) == 0xF0 && pos + 3 < static_cast<int>(str.size())) {
        return ((c & 0x07) << 18) | ((str[pos + 1] & 0x3F) << 12) |
               ((str[pos + 2] & 0x3F) << 6) | (str[pos + 3] & 0x3F);
    }
    return c;
}

bool is_word_codepoint(uint32_t cp) {
    if (cp == '_') return true;
    if (cp >= 'a' && cp <= 'z') return true;
    if (cp >= 'A' && cp <= 'Z') return true;
    if (cp >= '0' && cp <= '9') return true;
    if (cp >= 0x0400 && cp <= 0x04FF) return true;
    if (cp >= 0x0500 && cp <= 0x052F) return true;
    if (cp >= 0x00C0 && cp <= 0x00FF && cp != 0x00D7 && cp != 0x00F7) return true;
    if (cp >= 0x0100 && cp <= 0x017F) return true;
    if (cp >= 0x0180 && cp <= 0x024F) return true;
    if (cp >= 0x1E00 && cp <= 0x1EFF) return true;
    if (cp >= 0x0370 && cp <= 0x03FF) return true;
    if (cp >= 0x0590 && cp <= 0x05FF) return true;
    if (cp >= 0x0600 && cp <= 0x06FF) return true;
    if (cp >= 0x4E00 && cp <= 0x9FFF) return true;
    if (cp >= 0x3040 && cp <= 0x309F) return true;
    if (cp >= 0x30A0 && cp <= 0x30FF) return true;
    if (cp >= 0xAC00 && cp <= 0xD7AF) return true;
    return false;
}

bool is_directory(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) return false;
    return S_ISDIR(st.st_mode);
}

std::string get_resource_path(const std::string& filename) {
    char* base_path = SDL_GetBasePath();
    if (!base_path) {
        return filename;
    }

    std::string base(base_path);
    SDL_free(base_path);

#ifdef __APPLE__
    std::string resources_path = base + "../Resources/" + filename;
    if (std::filesystem::exists(resources_path)) {
        return resources_path;
    }
#endif

    std::string direct_path = base + filename;
    if (std::filesystem::exists(direct_path)) {
        return direct_path;
    }

#ifdef __linux__
    std::string share_path = "/usr/share/DeadEditor/" + filename;
    if (std::filesystem::exists(share_path)) {
        return share_path;
    }

    share_path = "/usr/local/share/DeadEditor/" + filename;
    if (std::filesystem::exists(share_path)) {
        return share_path;
    }
#endif

    if (std::filesystem::exists(filename)) {
        return filename;
    }

    return direct_path;
}

std::string get_config_path(const std::string& filename) {
    std::string config_dir;

#ifdef __APPLE__
    const char* home = getenv("HOME");
    if (home) {
        config_dir = std::string(home) + "/Library/Application Support/DeadEditor";
    }
#elif defined(__linux__)
    const char* xdg_config = getenv("XDG_CONFIG_HOME");
    if (xdg_config && xdg_config[0]) {
        config_dir = std::string(xdg_config) + "/DeadEditor";
    } else {
        const char* home = getenv("HOME");
        if (home) {
            config_dir = std::string(home) + "/.config/DeadEditor";
        }
    }
#else
    const char* appdata = getenv("APPDATA");
    if (appdata) {
        config_dir = std::string(appdata) + "/DeadEditor";
    }
#endif

    if (config_dir.empty()) {
        return filename;
    }

    if (!std::filesystem::exists(config_dir)) {
        std::filesystem::create_directories(config_dir);
    }

    return config_dir + "/" + filename;
}

void open_containing_folder(const std::string& path) {
    std::filesystem::path fs_path(path);
    std::string folder = fs_path.parent_path().string();
    if (std::filesystem::is_directory(fs_path)) {
        folder = fs_path.string();
    }

#ifdef __APPLE__
    std::string cmd = "open \"" + folder + "\"";
#elif defined(_WIN32)
    std::string cmd = "explorer \"" + folder + "\"";
#else
    std::string cmd = "xdg-open \"" + folder + "\" &";
#endif
    system(cmd.c_str());
}

std::string expand_tabs(const std::string& text, int tab_width) {
    std::string result;
    result.reserve(text.size());
    int column = 0;
    for (char c : text) {
        if (c == '\t') {
            int spaces = tab_width - (column % tab_width);
            result.append(spaces, ' ');
            column += spaces;
        } else {
            result.push_back(c);
            column++;
        }
    }
    return result;
}

int expanded_column(const std::string& text, int byte_pos, int tab_width) {
    int column = 0;
    int len = std::min(byte_pos, static_cast<int>(text.size()));
    for (int i = 0; i < len; i++) {
        if (text[i] == '\t') {
            column += tab_width - (column % tab_width);
        } else {
            column++;
        }
    }
    return column;
}
