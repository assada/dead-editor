#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <functional>
#include <cstdio>
#include <filesystem>
#include <format>
#include "Types.h"
#include "Constants.h"
#include "TextureCache.h"
#include "Layout.h"
#include "Utils.h"
#include "HandleTypes.h"

struct SearchResult {
    std::string file_path;
    std::string relative_path;
    LineIdx line;
    ColIdx col;
    std::string content;
};

enum class SearchState { Idle, Searching, Finished, Error };

class SearchOverlay {
public:
    using OnSelectCallback = std::function<void(const SearchResult&)>;
    using OnErrorCallback = std::function<void(const std::string&, const std::string&)>;

    bool visible = false;

    SearchOverlay() = default;
    ~SearchOverlay() { cancel_search(); }

    SearchOverlay(const SearchOverlay&) = delete;
    SearchOverlay& operator=(const SearchOverlay&) = delete;

    void set_root_path(const std::string& path) { root_path_ = path; }
    void set_on_error(OnErrorCallback callback) { on_error_ = std::move(callback); }

    void show() {
        if (root_path_.empty()) return;

        if (!check_ripgrep_available()) {
            if (on_error_) {
                on_error_("ripgrep not found", "Install ripgrep (rg) for global search");
            }
            return;
        }

        visible = true;
        SDL_StartTextInput();
        if (input_buffer_.size() >= MIN_QUERY_LENGTH) {
            start_search();
        }
    }

    void hide() {
        visible = false;
        cancel_search();
    }

    bool handle_key(const SDL_Event& event, OnSelectCallback on_select) {
        if (!visible) return false;

        SDL_Keycode key = event.key.keysym.sym;

        if (key == SDLK_ESCAPE) {
            hide();
            return true;
        }

        if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
            std::lock_guard lock(results_mutex_);
            if (!results_.empty() && selected_idx_ >= 0 &&
                selected_idx_ < static_cast<int>(results_.size())) {
                on_select(results_[selected_idx_]);
                hide();
            }
            return true;
        }

        if (key == SDLK_UP) {
            if (selected_idx_ > 0) {
                selected_idx_--;
                ensure_visible();
            }
            return true;
        }

        if (key == SDLK_DOWN) {
            std::lock_guard lock(results_mutex_);
            if (selected_idx_ < static_cast<int>(results_.size()) - 1) {
                selected_idx_++;
                ensure_visible();
            }
            return true;
        }

        if (key == SDLK_PAGEUP) {
            selected_idx_ = std::max(0, selected_idx_ - visible_count_);
            ensure_visible();
            return true;
        }

        if (key == SDLK_PAGEDOWN) {
            std::lock_guard lock(results_mutex_);
            selected_idx_ = std::min(static_cast<int>(results_.size()) - 1,
                                     selected_idx_ + visible_count_);
            if (selected_idx_ < 0) selected_idx_ = 0;
            ensure_visible();
            return true;
        }

        if (key == SDLK_BACKSPACE && !input_buffer_.empty()) {
            int prev = utf8_prev_char_start(input_buffer_, static_cast<int>(input_buffer_.size()));
            input_buffer_ = input_buffer_.substr(0, prev);
            if (input_buffer_.size() >= MIN_QUERY_LENGTH) {
                start_search();
            } else {
                cancel_search();
                std::lock_guard lock(results_mutex_);
                results_.clear();
                selected_idx_ = 0;
                scroll_offset_ = 0;
            }
            return true;
        }

        return true;
    }

    void handle_text_input(const char* text) {
        if (!visible) return;
        input_buffer_ += text;
        if (input_buffer_.size() >= MIN_QUERY_LENGTH) {
            start_search();
        }
    }

    void render(SDL_Renderer* renderer, const Layout& layout, TextureCache& cache,
                TTF_Font* font, int window_w, int window_h) {
        if (!visible) return;

        int line_h = TTF_FontHeight(font);
        int pad = layout.padding;
        int input_h = layout.scaled(INPUT_HEIGHT);

        int overlay_w = std::min(window_w - pad * 4, std::max(600, window_w * 3 / 4));
        int overlay_h = std::min(window_h - pad * 4, std::max(400, window_h * 3 / 4));

        int x = (window_w - overlay_w) / 2;
        int y = (window_h - overlay_h) / 2;

        int row_h = line_h * 2 + ROW_PADDING;
        int header_h = pad + input_h + pad + line_h + pad;
        int list_h = overlay_h - header_h - pad;
        visible_count_ = std::max(1, list_h / row_h);

        render_overlay_background(renderer, window_w, window_h);
        render_window(renderer, x, y, overlay_w, overlay_h);
        render_input_box(renderer, cache, font, x, y, overlay_w, pad, input_h);
        render_status(cache, x, y, pad, input_h);
        render_results(renderer, cache, font, x, y, overlay_w, overlay_h, pad, input_h, line_h);
    }

    bool has_ripgrep() const { return !ripgrep_path_.empty(); }

private:
    static constexpr int INPUT_HEIGHT = 44;
    static constexpr int ROW_PADDING = 12;
    static constexpr int ICON_WIDTH = 32;
    static constexpr size_t MIN_QUERY_LENGTH = 2;
    static constexpr size_t MAX_RESULTS = 1000;
    static constexpr size_t BATCH_SIZE = 50;

    std::string root_path_;
    std::string input_buffer_;
    std::vector<SearchResult> results_;
    mutable std::mutex results_mutex_;
    std::thread search_thread_;
    std::atomic<bool> searching_{false};
    std::atomic<bool> stop_requested_{false};
    std::atomic<SearchState> state_{SearchState::Idle};
    int selected_idx_ = 0;
    int scroll_offset_ = 0;
    mutable int visible_count_ = 10;
    std::string ripgrep_path_;
    bool ripgrep_checked_ = false;
    OnErrorCallback on_error_;

    bool check_ripgrep_available() {
        if (ripgrep_checked_) return !ripgrep_path_.empty();

        ripgrep_checked_ = true;
        ripgrep_path_ = find_ripgrep();
        return !ripgrep_path_.empty();
    }

    static std::string find_ripgrep() {
        std::vector<std::string> candidates = {
            "rg",
#ifdef __APPLE__
            "/opt/homebrew/bin/rg",
            "/usr/local/bin/rg",
#endif
            "/usr/bin/rg",
        };

        const char* home = std::getenv("HOME");
        if (home) {
            candidates.push_back(std::string(home) + "/.cargo/bin/rg");
        }

        for (const auto& path : candidates) {
            if (path == "rg") {
                std::string check_cmd = "command -v rg >/dev/null 2>&1";
                if (system(check_cmd.c_str()) == 0) {
                    return "rg";
                }
            } else if (std::filesystem::exists(path)) {
                return path;
            }
        }
        return "";
    }

    void ensure_visible() {
        if (selected_idx_ < scroll_offset_) {
            scroll_offset_ = selected_idx_;
        }
        if (selected_idx_ >= scroll_offset_ + visible_count_) {
            scroll_offset_ = selected_idx_ - visible_count_ + 1;
        }
    }

    void start_search() {
        cancel_search();
        if (input_buffer_.size() < MIN_QUERY_LENGTH) return;

        searching_ = true;
        stop_requested_ = false;
        state_ = SearchState::Searching;

        {
            std::lock_guard lock(results_mutex_);
            results_.clear();
            selected_idx_ = 0;
            scroll_offset_ = 0;
        }

        std::string query = input_buffer_;
        std::string root = root_path_;

        search_thread_ = std::thread([this, query, root]() {
            execute_search(query, root);
        });
    }

    void cancel_search() {
        stop_requested_ = true;
        searching_ = false;
        if (search_thread_.joinable()) {
            search_thread_.join();
        }
    }

    void execute_search(const std::string& query, const std::string& root) {
        std::string escaped_query = escape_shell_arg(query);
        std::string cmd = std::format(
            "{} --vimgrep --sortr=accessed --no-heading --smart-case --color never --max-count 100 ",
            escape_shell_arg(ripgrep_path_)
        ) + std::format(
            "--max-columns 500 {} {} 2>/dev/null",
            escaped_query, escape_shell_arg(root)
        );

        PipeHandle pipe(popen(cmd.c_str(), "r"));
        if (!pipe) {
            state_ = SearchState::Error;
            searching_ = false;
            return;
        }

        char buffer[2048];
        std::vector<SearchResult> batch;
        batch.reserve(BATCH_SIZE);
        size_t total_count = 0;

        while (fgets(buffer, sizeof(buffer), pipe.get()) && !stop_requested_.load()) {
            if (total_count >= MAX_RESULTS) break;

            std::string line(buffer);
            if (!line.empty() && line.back() == '\n') line.pop_back();
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) continue;

            auto result = parse_vimgrep_line(line, root);
            if (result.line > 0) {
                batch.push_back(std::move(result));
                total_count++;

                if (batch.size() >= BATCH_SIZE) {
                    std::lock_guard lock(results_mutex_);
                    results_.insert(results_.end(),
                                   std::make_move_iterator(batch.begin()),
                                   std::make_move_iterator(batch.end()));
                    batch.clear();
                }
            }
        }

        if (!batch.empty() && !stop_requested_.load()) {
            std::lock_guard lock(results_mutex_);
            results_.insert(results_.end(),
                           std::make_move_iterator(batch.begin()),
                           std::make_move_iterator(batch.end()));
        }

        state_ = SearchState::Finished;
        searching_ = false;
    }

    SearchResult parse_vimgrep_line(const std::string& line, const std::string& root) {
        SearchResult result{};

        size_t c1 = line.find(':');
        if (c1 == std::string::npos) return result;

        size_t c2 = line.find(':', c1 + 1);
        if (c2 == std::string::npos) return result;

        size_t c3 = line.find(':', c2 + 1);
        if (c3 == std::string::npos) return result;

        try {
            result.file_path = line.substr(0, c1);
            result.line = std::stoi(line.substr(c1 + 1, c2 - c1 - 1));
            result.col = std::stoi(line.substr(c2 + 1, c3 - c2 - 1));
            result.content = line.substr(c3 + 1);

            if (result.file_path.find(root) == 0) {
                result.relative_path = result.file_path.substr(root.size());
                if (!result.relative_path.empty() && result.relative_path[0] == '/') {
                    result.relative_path = result.relative_path.substr(1);
                }
            } else {
                result.relative_path = result.file_path;
            }

            trim_content(result.content);
        } catch (...) {
            result.line = 0;
        }

        return result;
    }

    static std::string escape_shell_arg(const std::string& arg) {
        std::string result = "'";
        for (char c : arg) {
            if (c == '\'') {
                result += "'\\''";
            } else {
                result += c;
            }
        }
        result += "'";
        return result;
    }

    static void trim_content(std::string& s) {
        size_t start = s.find_first_not_of(" \t");
        if (start == std::string::npos) {
            s.clear();
            return;
        }
        s = s.substr(start);
    }

    static std::string to_lower(const std::string& s) {
        std::string result = s;
        for (char& c : result) {
            if (c >= 'A' && c <= 'Z') c = c + ('a' - 'A');
        }
        return result;
    }

    static size_t find_case_insensitive(const std::string& haystack, const std::string& needle, size_t start = 0) {
        if (needle.empty()) return std::string::npos;
        std::string hay_lower = to_lower(haystack);
        std::string needle_lower = to_lower(needle);
        return hay_lower.find(needle_lower, start);
    }

    void render_highlighted_text(TextureCache& cache, TTF_Font* font, const std::string& text,
                                 const std::string& query, int x, int y, SDL_Color normal_color) {
        if (query.empty() || text.empty()) {
            cache.render_cached_text(text, normal_color, x, y);
            return;
        }

        int current_x = x;
        size_t pos = 0;

        while (pos < text.size()) {
            size_t match_pos = find_case_insensitive(text, query, pos);

            if (match_pos == std::string::npos) {
                std::string remaining = text.substr(pos);
                cache.render_cached_text(remaining, normal_color, current_x, y);
                break;
            }

            if (match_pos > pos) {
                std::string before = text.substr(pos, match_pos - pos);
                cache.render_cached_text(before, normal_color, current_x, y);
                int w = 0;
                TTF_SizeUTF8(font, before.c_str(), &w, nullptr);
                current_x += w;
            }

            std::string match = text.substr(match_pos, query.size());
            cache.render_cached_text(match, Colors::SYNTAX_STRING, current_x, y);
            int w = 0;
            TTF_SizeUTF8(font, match.c_str(), &w, nullptr);
            current_x += w;

            pos = match_pos + query.size();
        }
    }

    void render_overlay_background(SDL_Renderer* renderer, int window_w, int window_h) {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 150);
        SDL_Rect full_screen = {0, 0, window_w, window_h};
        SDL_RenderFillRect(renderer, &full_screen);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    }

    void render_window(SDL_Renderer* renderer, int x, int y, int w, int h) {
        SDL_SetRenderDrawColor(renderer, Colors::BG.r, Colors::BG.g, Colors::BG.b, 255);
        SDL_Rect win_rect = {x, y, w, h};
        SDL_RenderFillRect(renderer, &win_rect);

        SDL_SetRenderDrawColor(renderer, TAB_BORDER_COLOR.r, TAB_BORDER_COLOR.g, TAB_BORDER_COLOR.b, 255);
        SDL_RenderDrawRect(renderer, &win_rect);
    }

    void render_input_box(SDL_Renderer* renderer, TextureCache& cache, TTF_Font* font,
                          int x, int y, int w, int pad, int input_h) {
        SDL_SetRenderDrawColor(renderer, Colors::SEARCH_BG.r, Colors::SEARCH_BG.g, Colors::SEARCH_BG.b, 255);
        SDL_Rect input_rect = {x + pad, y + pad, w - pad * 2, input_h};
        SDL_RenderFillRect(renderer, &input_rect);

        SDL_SetRenderDrawColor(renderer, TAB_BORDER_COLOR.r, TAB_BORDER_COLOR.g, TAB_BORDER_COLOR.b, 255);
        SDL_RenderDrawRect(renderer, &input_rect);

        int line_h = TTF_FontHeight(font);
        int text_y = y + pad + (input_h - line_h) / 2;

        std::string icon = " ";
        cache.render_cached_text(icon, Colors::SYNTAX_FUNCTION, x + pad + pad / 2, text_y);

        std::string display_text = input_buffer_.empty() ? "Search in files..." : input_buffer_;
        SDL_Color text_color = input_buffer_.empty() ? Colors::LINE_NUM : Colors::TEXT;
        cache.render_cached_text(display_text, text_color, x + pad + pad / 2 + ICON_WIDTH, text_y);
    }

    void render_status(TextureCache& cache, int x, int y, int pad, int input_h) {
        std::string status;
        SDL_Color status_color = Colors::LINE_NUM;

        switch (state_.load()) {
            case SearchState::Searching:
                status = "Searching...";
                status_color = Colors::SYNTAX_FUNCTION;
                break;
            case SearchState::Finished: {
                std::lock_guard lock(results_mutex_);
                size_t count = results_.size();
                if (count == 0) {
                    status = "No results";
                } else if (count >= MAX_RESULTS) {
                    status = std::format("{} results (limited)", count);
                } else {
                    status = std::format("{} result{}", count, count == 1 ? "" : "s");
                }
                break;
            }
            case SearchState::Error:
                status = "Search error";
                status_color = Colors::TOAST_ERROR_ICON;
                break;
            default:
                break;
        }

        if (!status.empty()) {
            int status_y = y + pad + input_h + pad / 2;
            cache.render_cached_text(status, status_color, x + pad + pad / 2, status_y);
        }
    }

    void render_results(SDL_Renderer* renderer, TextureCache& cache, TTF_Font* font,
                        int x, int y, int w, int h, int pad, int input_h, int line_h) {
        int header_h = pad + input_h + pad + line_h + pad;
        int list_y = y + header_h;
        int list_h = h - header_h - pad;
        int row_h = line_h * 2 + ROW_PADDING;
        int scrollbar_w = 8;
        int content_w = w - pad * 2 - scrollbar_w - pad;

        SDL_Rect clip = {x + pad, list_y, w - pad * 2, list_h};
        SDL_RenderSetClipRect(renderer, &clip);

        std::lock_guard lock(results_mutex_);
        int draw_y = list_y;

        for (int i = scroll_offset_;
             i < static_cast<int>(results_.size()) && draw_y < list_y + list_h;
             ++i) {
            if (i == selected_idx_) {
                SDL_SetRenderDrawColor(renderer, Colors::SELECTION.r, Colors::SELECTION.g,
                                       Colors::SELECTION.b, Colors::SELECTION.a);
                SDL_Rect row = {x + pad, draw_y, w - pad * 2, row_h};
                SDL_RenderFillRect(renderer, &row);
            }

            const auto& res = results_[i];
            std::string location = std::format("{}:{}", res.relative_path, res.line);
            std::string content = res.content;

            size_t max_chars = static_cast<size_t>(content_w / 8);
            if (location.size() > max_chars) {
                location = "..." + location.substr(location.size() - max_chars + 3);
            }
            if (content.size() > max_chars) {
                content = content.substr(0, max_chars - 3) + "...";
            }

            int text_x = x + pad + pad / 2;
            int location_y = draw_y + ROW_PADDING / 2;
            int content_y = location_y + line_h + 2;

            cache.render_cached_text(location, Colors::SYNTAX_FUNCTION, text_x, location_y);
            render_highlighted_text(cache, font, content, input_buffer_, text_x, content_y, Colors::TEXT);

            draw_y += row_h;
        }

        SDL_RenderSetClipRect(renderer, nullptr);

        if (static_cast<int>(results_.size()) > visible_count_) {
            render_scrollbar(renderer, x + w - pad - scrollbar_w, list_y, scrollbar_w, list_h,
                            static_cast<int>(results_.size()), visible_count_, scroll_offset_);
        }
    }

    void render_scrollbar(SDL_Renderer* renderer, int x, int y, int w, int h,
                          int total_items, int visible_items, int scroll_pos) {
        SDL_SetRenderDrawColor(renderer, Colors::SCROLLBAR_BG.r, Colors::SCROLLBAR_BG.g,
                               Colors::SCROLLBAR_BG.b, 255);
        SDL_Rect bg = {x, y, w, h};
        SDL_RenderFillRect(renderer, &bg);

        if (total_items <= visible_items) return;

        int thumb_h = std::max(20, h * visible_items / total_items);
        int max_scroll = total_items - visible_items;
        int thumb_y = y + (h - thumb_h) * scroll_pos / max_scroll;

        SDL_SetRenderDrawColor(renderer, Colors::SCROLLBAR_THUMB.r, Colors::SCROLLBAR_THUMB.g,
                               Colors::SCROLLBAR_THUMB.b, 255);
        SDL_Rect thumb = {x, thumb_y, w, thumb_h};
        SDL_RenderFillRect(renderer, &thumb);
    }
};
