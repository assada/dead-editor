#pragma once

#include "Types.h"
#include "TextDocument.h"
#include "Syntax.h"
#include "Layout.h"
#include "TextureCache.h"
#include "Constants.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <unordered_map>
#include <unordered_set>
#include <functional>

class EditorView {
public:
    int scroll_x = 0;
    int scroll_y = 0;
    int line_height = 20;

    bool scrollbar_dragging = false;
    int scrollbar_drag_offset = 0;
    bool scrollbar_hovered = false;
    int scaled_scrollbar_width = SCROLLBAR_WIDTH;
    int scaled_scrollbar_min_thumb = SCROLLBAR_MIN_THUMB_HEIGHT;

    SyntaxHighlighter highlighter;
    std::unordered_map<size_t, std::vector<Token>> token_cache;
    std::unordered_map<LineIdx, std::vector<Token>> viewport_tokens_buffer;

    std::vector<HighlightRange> highlight_occurrences;
    std::string highlighted_identifier;
    LineIdx last_highlight_line = -1;
    ColIdx last_highlight_col = -1;

    std::vector<FoldRegion> fold_regions;
    std::unordered_set<LineIdx> folded_lines;

    bool syntax_dirty = true;
    Uint32 last_edit_time = 0;

    EditorView() = default;

    void init_for_file(const std::string& filepath, const TextDocument& doc);
    void mark_syntax_dirty();

    void rebuild_syntax(const TextDocument& doc);
    void prefetch_viewport_tokens(LineIdx start_line, int visible_count, const TextDocument& doc);
    const std::vector<Token>& get_line_tokens(size_t line_idx);

    bool is_line_folded(LineIdx line) const;
    bool is_fold_start(LineIdx line) const;
    bool is_fold_start_folded(LineIdx line) const;
    LineIdx get_fold_end_line(LineIdx start_line) const;
    FoldRegion* get_fold_region_at_line(LineIdx line);
    bool toggle_fold_at_line(LineIdx line);
    void fold_all();
    void unfold_all();
    void update_fold_regions(const TextDocument& doc);
    void update_folded_lines();

    int get_total_visible_lines(const TextDocument& doc) const;
    int count_visible_lines_between(LineIdx from_line, LineIdx to_line) const;
    LineIdx get_nth_visible_line_from(LineIdx start_line, int n, const TextDocument& doc) const;
    LineIdx get_first_visible_line_from(LineIdx line) const;
    LineIdx get_next_visible_line(LineIdx from_line, int direction, const TextDocument& doc) const;

    void get_scrollbar_metrics(int visible_height, int min_thumb_height, int& thumb_height, int& thumb_y,
                               const TextDocument& doc) const;
    bool is_point_in_scrollbar(int x, int y, int x_offset, int y_offset, int visible_width, int visible_height) const;
    void scroll_to_line(LineIdx target_line, const TextDocument& doc);

    void ensure_cursor_visible(LineIdx cursor_line, int visible_lines, const TextDocument& doc);
    void ensure_visible_x(int cursor_pixel_x, int visible_width, int margin);

    void handle_scroll(int wheel_x, int wheel_y, int char_w, bool shift_held, const TextDocument& doc);

    void render(SDL_Renderer* renderer, TTF_Font* font, TextureCache& texture_cache,
                const TextDocument& doc,
                LineIdx cursor_line, ColIdx cursor_col,
                bool sel_active, LineIdx sel_start_line, ColIdx sel_start_col,
                const std::string& search_query,
                int x_offset, int y_offset, int visible_width, int visible_height,
                int window_w, int char_width,
                bool has_focus, bool is_file_open, bool cursor_visible,
                const Layout& layout,
                std::function<SDL_Color(TokenType)> syntax_color_func);

    void clear_caches();

private:
    void collect_fold_regions_recursive(TSNode node, const TextDocument& doc);
    bool is_foldable_node(TSNode node) const;
};
