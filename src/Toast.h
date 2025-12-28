#pragma once

#include <SDL2/SDL.h>
#include <string>
#include <vector>
#include <functional>
#include "Constants.h"
#include "Layout.h"
#include "TextureCache.h"

enum class ToastType {
    Info,
    Warning,
    Error,
    Success
};

struct Toast {
    std::string title;
    std::string message;
    ToastType type;
    Uint32 created_at;
    Uint32 delay_ms;
    int id;

    bool is_expired(Uint32 now) const {
        return (now - created_at) >= delay_ms;
    }

    float get_progress(Uint32 now) const {
        Uint32 elapsed = now - created_at;
        return static_cast<float>(elapsed) / static_cast<float>(delay_ms);
    }
};

class ToastManager {
public:
    void set_layout(const Layout* l) { layout_ = l; }

    void show(const std::string& title, const std::string& message, ToastType type, Uint32 delay_ms = 3000) {
        toasts_.push_back({
            .title = title,
            .message = message,
            .type = type,
            .created_at = SDL_GetTicks(),
            .delay_ms = delay_ms,
            .id = next_id_++
        });
    }

    void show_info(const std::string& title, const std::string& message, Uint32 delay_ms = 3000) {
        show(title, message, ToastType::Info, delay_ms);
    }

    void show_success(const std::string& title, const std::string& message, Uint32 delay_ms = 3000) {
        show(title, message, ToastType::Success, delay_ms);
    }

    void show_warning(const std::string& title, const std::string& message, Uint32 delay_ms = 4000) {
        show(title, message, ToastType::Warning, delay_ms);
    }

    void show_error(const std::string& title, const std::string& message, Uint32 delay_ms = 5000) {
        show(title, message, ToastType::Error, delay_ms);
    }

    void update() {
        if (toasts_.empty()) return;

        Uint32 now = SDL_GetTicks();
        toasts_.erase(
            std::remove_if(toasts_.begin(), toasts_.end(),
                [now](const Toast& t) { return t.is_expired(now); }),
            toasts_.end()
        );
    }

    bool handle_click(int x, int y, int window_w, int window_h) {
        if (toasts_.empty() || !layout_) return false;

        int toast_y = window_h - layout_->status_bar_height - TOAST_MARGIN;

        for (auto it = toasts_.rbegin(); it != toasts_.rend(); ++it) {
            int toast_height = calculate_toast_height(*it);
            toast_y -= toast_height + TOAST_SPACING;

            SDL_Rect rect = {
                window_w - TOAST_WIDTH - TOAST_MARGIN,
                toast_y,
                TOAST_WIDTH,
                toast_height
            };

            if (x >= rect.x && x < rect.x + rect.w &&
                y >= rect.y && y < rect.y + rect.h) {
                toasts_.erase(std::next(it).base());
                return true;
            }
        }
        return false;
    }

    void render(SDL_Renderer* renderer, TextureCache& texture_cache,
                int window_w, int window_h, int line_height) {
        if (toasts_.empty() || !layout_) return;

        line_height_ = line_height;
        Uint32 now = SDL_GetTicks();
        int toast_y = window_h - layout_->status_bar_height - TOAST_MARGIN;

        for (auto it = toasts_.rbegin(); it != toasts_.rend(); ++it) {
            const Toast& toast = *it;
            int toast_height = calculate_toast_height(toast);
            toast_y -= toast_height + TOAST_SPACING;

            render_toast(renderer, texture_cache, toast,
                        window_w - TOAST_WIDTH - TOAST_MARGIN, toast_y,
                        TOAST_WIDTH, toast_height, now);
        }
    }

    bool empty() const { return toasts_.empty(); }

private:
    static constexpr int TOAST_WIDTH = 380;
    static constexpr int TOAST_MARGIN = 16;
    static constexpr int TOAST_SPACING = 10;
    static constexpr int TOAST_PADDING = 16;
    static constexpr int TOAST_INDICATOR_WIDTH = 5;
    static constexpr int TOAST_PROGRESS_HEIGHT = 4;
    static constexpr int TOAST_ICON_SIZE = 28;
    static constexpr int TOAST_LINE_GAP = 6;

    std::vector<Toast> toasts_;
    const Layout* layout_ = nullptr;
    int next_id_ = 0;
    int line_height_ = 22;

    int calculate_toast_height(const Toast& toast) const {
        int content_height = 0;
        if (!toast.title.empty()) content_height += line_height_;
        if (!toast.message.empty()) content_height += line_height_;
        if (!toast.title.empty() && !toast.message.empty()) content_height += TOAST_LINE_GAP;

        int height = TOAST_PADDING * 2 + content_height + TOAST_PROGRESS_HEIGHT;
        return std::max(height, 64);
    }

    SDL_Color get_indicator_color(ToastType type) const {
        switch (type) {
            case ToastType::Info:    return Colors::TOAST_INFO_INDICATOR;
            case ToastType::Success: return Colors::TOAST_SUCCESS_INDICATOR;
            case ToastType::Warning: return Colors::TOAST_WARNING_INDICATOR;
            case ToastType::Error:   return Colors::TOAST_ERROR_INDICATOR;
        }
        return Colors::TOAST_INFO_INDICATOR;
    }

    SDL_Color get_icon_color(ToastType type) const {
        switch (type) {
            case ToastType::Info:    return Colors::TOAST_INFO_ICON;
            case ToastType::Success: return Colors::TOAST_SUCCESS_ICON;
            case ToastType::Warning: return Colors::TOAST_WARNING_ICON;
            case ToastType::Error:   return Colors::TOAST_ERROR_ICON;
        }
        return Colors::TOAST_INFO_ICON;
    }

    std::string get_icon(ToastType type) const {
        switch (type) {
            case ToastType::Info:    return "";
            case ToastType::Success: return "";
            case ToastType::Warning: return "";
            case ToastType::Error:   return "";
        }
        return "";
    }

    void render_toast(SDL_Renderer* renderer, TextureCache& texture_cache,
                      const Toast& toast, int x, int y, int w, int h, Uint32 now) {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, Colors::TOAST_BG.r, Colors::TOAST_BG.g, Colors::TOAST_BG.b, Colors::TOAST_BG.a);
        SDL_Rect bg = {x, y, w, h};
        SDL_RenderFillRect(renderer, &bg);

        SDL_Color indicator_color = get_indicator_color(toast.type);
        SDL_SetRenderDrawColor(renderer, indicator_color.r, indicator_color.g, indicator_color.b, 255);
        SDL_Rect indicator = {x, y, TOAST_INDICATOR_WIDTH, h - TOAST_PROGRESS_HEIGHT};
        SDL_RenderFillRect(renderer, &indicator);

        SDL_SetRenderDrawColor(renderer, Colors::TOAST_BORDER.r, Colors::TOAST_BORDER.g, Colors::TOAST_BORDER.b, 255);
        SDL_RenderDrawRect(renderer, &bg);

        int content_height = h - TOAST_PADDING * 2 - TOAST_PROGRESS_HEIGHT;
        int text_block_height = 0;
        if (!toast.title.empty()) text_block_height += line_height_;
        if (!toast.message.empty()) text_block_height += line_height_;
        if (!toast.title.empty() && !toast.message.empty()) text_block_height += TOAST_LINE_GAP;

        int content_x = x + TOAST_INDICATOR_WIDTH + TOAST_PADDING;
        int content_y = y + TOAST_PADDING + (content_height - text_block_height) / 2;

        int icon_y = y + (h - TOAST_PROGRESS_HEIGHT - line_height_) / 2;
        SDL_Color icon_color = get_icon_color(toast.type);
        std::string icon = get_icon(toast.type);
        texture_cache.render_cached_text(icon, icon_color, content_x, icon_y);

        int text_x = content_x + TOAST_ICON_SIZE;

        if (!toast.title.empty()) {
            texture_cache.render_cached_text(toast.title, Colors::TOAST_TEXT, text_x, content_y);
            content_y += line_height_ + TOAST_LINE_GAP;
        }

        if (!toast.message.empty()) {
            texture_cache.render_cached_text(toast.message, Colors::TOAST_TEXT_DIM, text_x, content_y);
        }

        float progress = 1.0f - toast.get_progress(now);
        int progress_width = static_cast<int>(static_cast<float>(w) * progress);

        SDL_SetRenderDrawColor(renderer, Colors::TOAST_PROGRESS_BG.r, Colors::TOAST_PROGRESS_BG.g, Colors::TOAST_PROGRESS_BG.b, 255);
        SDL_Rect progress_bg = {x, y + h - TOAST_PROGRESS_HEIGHT, w, TOAST_PROGRESS_HEIGHT};
        SDL_RenderFillRect(renderer, &progress_bg);

        SDL_SetRenderDrawColor(renderer, indicator_color.r, indicator_color.g, indicator_color.b, 180);
        SDL_Rect progress_bar = {x, y + h - TOAST_PROGRESS_HEIGHT, progress_width, TOAST_PROGRESS_HEIGHT};
        SDL_RenderFillRect(renderer, &progress_bar);
    }
};
