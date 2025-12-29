#include "EditorView.h"
#include "RenderUtils.h"
#include "Utils.h"
#include <algorithm>
#include <cmath>
#include <format>

void EditorView::init_for_file(const std::string& filepath, const TextDocument& doc) {
    clear_caches();
    highlighter.tree.reset();
    highlighter.set_language_for_file(filepath, doc.lines, doc.offset_manager);
    syntax_dirty = true;
}

void EditorView::mark_syntax_dirty() {
    syntax_dirty = true;
    last_edit_time = SDL_GetTicks();
    token_cache.clear();
}

void EditorView::rebuild_syntax(const TextDocument& doc) {
    if (!syntax_dirty) return;

    highlighter.parse_incremental(doc.lines, doc.offset_manager);
    token_cache.clear();

    if (doc.lines.size() < MAX_LINES_FOR_FOLDING) {
        update_fold_regions(doc);
    } else {
        fold_regions.clear();
        folded_lines.clear();
    }

    syntax_dirty = false;
}

void EditorView::prefetch_viewport_tokens(LineIdx start_line, int visible_count, const TextDocument& doc) {
    start_line = std::max(LineIdx{0}, start_line);

    int lines_found = 0;
    int current_line = start_line;
    int max_lines = static_cast<int>(doc.lines.size());

    while (current_line < max_lines && lines_found < visible_count) {
        if (!is_line_folded(current_line)) {
            lines_found++;
        }
        current_line++;
    }

    int end_line = current_line;

    bool need_fetch = false;
    for (int i = start_line; i < end_line; i++) {
        if (is_line_folded(i)) continue;

        if (token_cache.find(i) == token_cache.end()) {
            need_fetch = true;
            break;
        }
    }

    if (need_fetch) {
        highlighter.get_viewport_tokens(start_line, end_line, doc.offset_manager, doc.lines, viewport_tokens_buffer);
        for (auto& [line_idx, tokens] : viewport_tokens_buffer) {
            token_cache[line_idx] = std::move(tokens);
        }
        for (int i = start_line; i < end_line; i++) {
            if (token_cache.find(i) == token_cache.end()) {
                token_cache[i] = {};
            }
        }
    }
}

const std::vector<Token>& EditorView::get_line_tokens(size_t line_idx) {
    static const std::vector<Token> empty_tokens;
    auto it = token_cache.find(line_idx);
    if (it != token_cache.end()) {
        return it->second;
    }
    return empty_tokens;
}

bool EditorView::is_line_folded(LineIdx line) const {
    return folded_lines.count(line) > 0;
}

bool EditorView::is_fold_start(LineIdx line) const {
    for (const auto& fr : fold_regions) {
        if (fr.start_line == line) {
            return true;
        }
    }
    return false;
}

bool EditorView::is_fold_start_folded(LineIdx line) const {
    for (const auto& fr : fold_regions) {
        if (fr.start_line == line) {
            return fr.folded;
        }
    }
    return false;
}

LineIdx EditorView::get_fold_end_line(LineIdx start_line) const {
    for (const auto& fr : fold_regions) {
        if (fr.start_line == start_line) {
            return fr.end_line;
        }
    }
    return start_line;
}

FoldRegion* EditorView::get_fold_region_at_line(LineIdx line) {
    for (auto& fr : fold_regions) {
        if (fr.start_line == line) {
            return &fr;
        }
    }
    return nullptr;
}

bool EditorView::toggle_fold_at_line(LineIdx line) {
    FoldRegion* fr = get_fold_region_at_line(line);
    if (!fr) return false;
    fr->folded = !fr->folded;
    update_folded_lines();
    return true;
}

void EditorView::fold_all() {
    for (auto& fr : fold_regions) {
        fr.folded = true;
    }
    update_folded_lines();
}

void EditorView::unfold_all() {
    for (auto& fr : fold_regions) {
        fr.folded = false;
    }
    update_folded_lines();
}

void EditorView::update_fold_regions(const TextDocument& doc) {
    if (!highlighter.tree) return;

    std::unordered_set<int> old_folded;
    for (const auto& fr : fold_regions) {
        if (fr.folded) {
            old_folded.insert(fr.start_line);
        }
    }

    fold_regions.clear();
    TSNode root = ts_tree_root_node(highlighter.tree.get());
    collect_fold_regions_recursive(root, doc);

    for (auto& fr : fold_regions) {
        if (old_folded.count(fr.start_line)) {
            fr.folded = true;
        }
    }
    update_folded_lines();
}

void EditorView::update_folded_lines() {
    folded_lines.clear();
    for (const auto& fr : fold_regions) {
        if (fr.folded) {
            for (int line = fr.start_line + 1; line <= fr.end_line; line++) {
                folded_lines.insert(line);
            }
        }
    }
}

bool EditorView::is_foldable_node(TSNode node) const {
    if (ts_node_is_null(node)) return false;
    const char* type = ts_node_type(node);
    return strcmp(type, "function_definition") == 0 ||
           strcmp(type, "compound_statement") == 0 ||
           strcmp(type, "class_specifier") == 0 ||
           strcmp(type, "struct_specifier") == 0 ||
           strcmp(type, "namespace_definition") == 0 ||
           strcmp(type, "if_statement") == 0 ||
           strcmp(type, "for_statement") == 0 ||
           strcmp(type, "while_statement") == 0 ||
           strcmp(type, "switch_statement") == 0 ||
           strcmp(type, "enum_specifier") == 0 ||
           strcmp(type, "comment") == 0 ||
           strcmp(type, "class_definition") == 0 ||
           strcmp(type, "function_declaration") == 0 ||
           strcmp(type, "method_definition") == 0 ||
           strcmp(type, "arrow_function") == 0 ||
           strcmp(type, "class_declaration") == 0 ||
           strcmp(type, "try_statement") == 0 ||
           strcmp(type, "catch_clause") == 0 ||
           strcmp(type, "with_statement") == 0 ||
           strcmp(type, "do_statement") == 0 ||
           strcmp(type, "statement_block") == 0 ||
           strcmp(type, "object") == 0 ||
           strcmp(type, "array") == 0 ||
           strcmp(type, "block") == 0 ||
           strcmp(type, "if_expression") == 0 ||
           strcmp(type, "match_expression") == 0 ||
           strcmp(type, "else_clause") == 0 ||
           strcmp(type, "elif_clause") == 0 ||
           strcmp(type, "except_clause") == 0 ||
           strcmp(type, "finally_clause") == 0 ||
           strcmp(type, "for_in_statement") == 0 ||
           strcmp(type, "repeat_statement") == 0 ||
           strcmp(type, "function_statement") == 0 ||
           strcmp(type, "local_function") == 0 ||
           strcmp(type, "fenced_code_block") == 0 ||
           strcmp(type, "block_mapping") == 0 ||
           strcmp(type, "block_sequence") == 0 ||
           strcmp(type, "table") == 0 ||
           strcmp(type, "inline_table") == 0 ||
           strcmp(type, "array_of_tables") == 0 ||
           strcmp(type, "rule_set") == 0 ||
           strcmp(type, "media_statement") == 0 ||
           strcmp(type, "keyframes_statement") == 0 ||
           strcmp(type, "element") == 0;
}

void EditorView::collect_fold_regions_recursive(TSNode node, [[maybe_unused]] const TextDocument& doc) {
    if (ts_node_is_null(node)) return;

    if (is_foldable_node(node)) {
        TSPoint start = ts_node_start_point(node);
        TSPoint end = ts_node_end_point(node);
        if (end.row > start.row) {
            bool already_exists = false;
            for (const auto& fr : fold_regions) {
                if (fr.start_line == static_cast<int>(start.row)) {
                    already_exists = true;
                    break;
                }
            }
            if (!already_exists) {
                fold_regions.push_back({static_cast<int>(start.row), static_cast<int>(end.row), false});
            }
        }
    }

    uint32_t child_count = ts_node_child_count(node);
    for (uint32_t i = 0; i < child_count; i++) {
        collect_fold_regions_recursive(ts_node_child(node, i), doc);
    }
}

int EditorView::get_total_visible_lines(const TextDocument& doc) const {
    int count = 0;
    for (int i = 0; i < static_cast<int>(doc.lines.size()); i++) {
        if (!is_line_folded(i)) {
            count++;
        }
    }
    return count;
}

int EditorView::count_visible_lines_between(LineIdx from_line, LineIdx to_line) const {
    int count = 0;
    LineIdx start = std::min(from_line, to_line);
    LineIdx end = std::max(from_line, to_line);
    for (LineIdx i = start; i <= end; i++) {
        if (!is_line_folded(i)) {
            count++;
        }
    }
    return count;
}

LineIdx EditorView::get_nth_visible_line_from(LineIdx start_line, int n, const TextDocument& doc) const {
    int count = 0;
    LineIdx line = start_line;
    int direction = (n >= 0) ? 1 : -1;
    int target = std::abs(n);
    while (line >= 0 && line < static_cast<LineIdx>(doc.lines.size())) {
        if (!is_line_folded(line)) {
            if (count == target) return line;
            count++;
        }
        line += direction;
    }
    return std::clamp(line - direction, LineIdx{0}, static_cast<LineIdx>(doc.lines.size()) - 1);
}

LineIdx EditorView::get_first_visible_line_from(LineIdx line) const {
    while (line > 0 && is_line_folded(line)) {
        line--;
    }
    return line;
}

LineIdx EditorView::get_next_visible_line(LineIdx from_line, int direction, const TextDocument& doc) const {
    LineIdx line = from_line + direction;
    while (line >= 0 && line < static_cast<LineIdx>(doc.lines.size())) {
        if (!is_line_folded(line)) {
            return line;
        }
        line += direction;
    }
    return from_line;
}

float EditorView::get_max_scroll_pixels(const TextDocument& doc) const {
    int total_visible = get_total_visible_lines(doc);
    return std::max(0.0f, (float)((total_visible - 1) * line_height));
}

void EditorView::clamp_scroll_values(float max_scroll) {
    if (precise_scroll_y < 0.0) {
        precise_scroll_y = 0.0;
        velocity_y = 0.0;
        if (scroll_state == ScrollState::Momentum) scroll_state = ScrollState::Idle;
    } else if (precise_scroll_y > max_scroll) {
        precise_scroll_y = max_scroll;
        velocity_y = 0.0f;
        if (scroll_state == ScrollState::Momentum) scroll_state = ScrollState::Idle;
    }
}

void EditorView::sync_scroll_position(const TextDocument& /*doc*/) {
    int visual_lines_above = count_visible_lines_between(0, scroll_y);
    precise_scroll_y = static_cast<double>(visual_lines_above * line_height);
    target_scroll_y = precise_scroll_y;
    velocity_y = 0.0;
    scroll_state = ScrollState::Idle;
}

void EditorView::get_scrollbar_metrics(int visible_height, int min_thumb_height, int& thumb_height, int& thumb_y,
                                        const TextDocument& doc) const {
    int total_visible = get_total_visible_lines(doc);
    int visible_lines = visible_height / line_height;

    if (total_visible <= visible_lines) {
        thumb_height = visible_height;
        thumb_y = 0;
        return;
    }

    float thumb_ratio = static_cast<float>(visible_lines) / total_visible;
    thumb_height = std::max(min_thumb_height, static_cast<int>(visible_height * thumb_ratio));

    float max_scroll = std::max(1.0f, get_max_scroll_pixels(doc));
    float scroll_ratio = std::clamp(static_cast<float>(precise_scroll_y) / max_scroll, 0.0f, 1.0f);
    thumb_y = static_cast<int>(scroll_ratio * (visible_height - thumb_height));
}

bool EditorView::is_point_in_scrollbar(int x, int y, int x_offset, int y_offset, int visible_width, int visible_height) const {
    int scrollbar_x = x_offset + visible_width - scaled_scrollbar_width;
    return x >= scrollbar_x && x < x_offset + visible_width &&
           y >= y_offset && y < y_offset + visible_height;
}

void EditorView::scroll_to_line(LineIdx target_line, const TextDocument& doc) {
    target_line = std::max(LineIdx{0}, std::min(target_line, static_cast<LineIdx>(doc.lines.size()) - 1));
    scroll_y = get_first_visible_line_from(target_line);
    sync_scroll_position(doc);
}

void EditorView::ensure_cursor_visible(LineIdx cursor_line, int visible_lines, const TextDocument& doc) {
    int cursor_visual = get_first_visible_line_from(cursor_line);
    int lines_from_top = count_visible_lines_between(scroll_y, cursor_visual);
    int old_scroll_y = scroll_y;

    if (cursor_visual < scroll_y) {
        scroll_y = cursor_visual;
    } else if (lines_from_top >= visible_lines) {
        int target_from_top = visible_lines - 1;
        int lines_to_skip = lines_from_top - target_from_top;
        scroll_y = get_nth_visible_line_from(scroll_y, lines_to_skip, doc);
    }
    scroll_y = get_first_visible_line_from(scroll_y);

    if (scroll_y != old_scroll_y) {
        sync_scroll_position(doc);
    }
}

void EditorView::ensure_visible_x(int cursor_pixel_x, int visible_width, int margin) {
    if (cursor_pixel_x - scroll_x < margin) {
        scroll_x = std::max(0, cursor_pixel_x - margin);
    }
    if (cursor_pixel_x - scroll_x > visible_width - margin) {
        scroll_x = cursor_pixel_x - visible_width + margin;
    }
}

void EditorView::handle_scroll(float wheel_x, float wheel_y, int char_w, bool shift_held, const TextDocument& doc) {
    if (shift_held) {
        wheel_x = wheel_y;
        wheel_y = 0;
    }

    constexpr float SCROLL_MULTIPLIER = 50.0f;

    if (std::abs(wheel_x) > 0.0001f) {
        target_scroll_x += wheel_x * SCROLL_MULTIPLIER;
        if (target_scroll_x < 0.0) target_scroll_x = 0.0;
    }

    if (std::abs(wheel_y) > 0.0001f) {
        target_scroll_y -= wheel_y * SCROLL_MULTIPLIER;

        float max_scroll = get_max_scroll_pixels(doc);
        if (target_scroll_y < 0.0) target_scroll_y = 0.0;
        if (target_scroll_y > max_scroll) target_scroll_y = max_scroll;
    }
}

void EditorView::update_smooth_scroll(const TextDocument& doc) {
    constexpr double LERP_FACTOR = 0.25;

    double diff_y = target_scroll_y - precise_scroll_y;
    if (std::abs(diff_y) > 0.5) {
        precise_scroll_y += diff_y * LERP_FACTOR;
    } else if (std::abs(diff_y) > 0.001) {
        precise_scroll_y = target_scroll_y;
    }

    double diff_x = target_scroll_x - precise_scroll_x;
    if (std::abs(diff_x) > 0.5) {
        precise_scroll_x += diff_x * LERP_FACTOR;
    } else if (std::abs(diff_x) > 0.001) {
        precise_scroll_x = target_scroll_x;
    }

    float max_scroll_y = get_max_scroll_pixels(doc);
    if (precise_scroll_y < 0.0) {
        precise_scroll_y = 0.0;
        target_scroll_y = 0.0;
    } else if (precise_scroll_y > max_scroll_y) {
        precise_scroll_y = max_scroll_y;
        target_scroll_y = max_scroll_y;
    }

    if (precise_scroll_x < 0.0) {
        precise_scroll_x = 0.0;
        target_scroll_x = 0.0;
    }

    scroll_y = static_cast<int>(precise_scroll_y / line_height);
    if (scroll_y < 0) scroll_y = 0;
    int max_lines = static_cast<int>(doc.lines.size());
    if (scroll_y >= max_lines && max_lines > 0) scroll_y = max_lines - 1;

    scroll_x = static_cast<int>(precise_scroll_x);
}

void EditorView::render(SDL_Renderer* renderer, TTF_Font* font, TextureCache& texture_cache,
                        const TextDocument& doc,
                        LineIdx cursor_line, ColIdx cursor_col,
                        bool sel_active, LineIdx sel_start_line, ColIdx sel_start_col,
                        const std::string& search_query,
                        int x_offset, int y_offset, int visible_width, int visible_height,
                        int window_w, int char_width,
                        bool has_focus, bool is_file_open, bool cursor_visible,
                        const Layout& layout,
                        std::function<SDL_Color(TokenType)> syntax_color_func) {

    scaled_scrollbar_width = layout.scrollbar_width;
    scaled_scrollbar_min_thumb = layout.scrollbar_min_thumb_height;

    int visible_end_y = y_offset + visible_height;
    int visible_lines = visible_height / line_height;
    int text_x = x_offset + GUTTER_WIDTH + PADDING - scroll_x;

    int pixel_offset = static_cast<int>(precise_scroll_y) % line_height;
    int y = y_offset - pixel_offset;

    SDL_SetRenderDrawColor(renderer, Colors::GUTTER.r, Colors::GUTTER.g, Colors::GUTTER.b, 255);
    SDL_Rect gutter_rect = {x_offset, y_offset, GUTTER_WIDTH, visible_height};
    SDL_RenderFillRect(renderer, &gutter_rect);

    SDL_Rect gutter_clip = {x_offset, y_offset, GUTTER_WIDTH, visible_height};
    SDL_RenderSetClipRect(renderer, &gutter_clip);

    for (int i = scroll_y; i < static_cast<int>(doc.lines.size()) && y < visible_end_y; i++) {
        if (is_line_folded(i)) continue;

        bool is_cursor_line_flag = (i == cursor_line) && is_file_open;
        if (is_cursor_line_flag && has_focus) {
            SDL_SetRenderDrawColor(renderer, Colors::ACTIVE_LINE.r, Colors::ACTIVE_LINE.g, Colors::ACTIVE_LINE.b, 255);
            SDL_Rect active_gutter_rect = {x_offset, y, GUTTER_WIDTH, line_height};
            SDL_RenderFillRect(renderer, &active_gutter_rect);
        }

        std::string line_num = std::format("{}", i + 1);
        SDL_Color num_color = is_cursor_line_flag ? Colors::LINE_NUM_ACTIVE : Colors::LINE_NUM;
        texture_cache.render_cached_text_right_aligned(line_num, num_color, x_offset + GUTTER_WIDTH - 8, y);

        if (is_fold_start(i)) {
            const char* marker = is_fold_start_folded(i) ? "▶" : "▼";
            texture_cache.render_cached_text(marker, Colors::FOLD_INDICATOR, x_offset + 4, y);
        }
        y += line_height;
    }

    SDL_RenderSetClipRect(renderer, nullptr);
    SDL_Rect text_clip = {x_offset + GUTTER_WIDTH, y_offset, visible_width - GUTTER_WIDTH, visible_height};
    SDL_RenderSetClipRect(renderer, &text_clip);

    if (syntax_dirty) {
        bool is_large_file = doc.lines.size() > LARGE_FILE_LINES;
        if (!is_large_file || (SDL_GetTicks() - last_edit_time > SYNTAX_DEBOUNCE_MS)) {
            const_cast<EditorView*>(this)->rebuild_syntax(doc);
        }
    }
    const_cast<EditorView*>(this)->prefetch_viewport_tokens(scroll_y, visible_lines + 5, doc);

    y = y_offset - pixel_offset;

    for (int i = scroll_y; i < static_cast<int>(doc.lines.size()) && y < visible_end_y; i++) {
        if (is_line_folded(i)) continue;

        if (i == cursor_line && is_file_open && has_focus) {
            SDL_SetRenderDrawColor(renderer, Colors::ACTIVE_LINE.r, Colors::ACTIVE_LINE.g, Colors::ACTIVE_LINE.b, 255);
            SDL_Rect active_line_rect = {x_offset + GUTTER_WIDTH, y, visible_width - GUTTER_WIDTH, line_height};
            SDL_RenderFillRect(renderer, &active_line_rect);
        }

        for (const auto& hl : highlight_occurrences) {
            if (hl.line == i) {
                std::string expanded_line = expand_tabs(doc.lines[i]);
                int exp_start = expanded_column(doc.lines[i], hl.start_col);
                int exp_end = expanded_column(doc.lines[i], hl.end_col);
                int hl_x_start = text_x;
                if (exp_start > 0) {
                    int w = 0;
                    TTF_SizeUTF8(font, expanded_line.substr(0, exp_start).c_str(), &w, nullptr);
                    hl_x_start += w;
                }
                int hl_w = 0;
                TTF_SizeUTF8(font, expanded_line.substr(exp_start, exp_end - exp_start).c_str(), &hl_w, nullptr);
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(renderer, Colors::OCCURRENCE_HIGHLIGHT.r, Colors::OCCURRENCE_HIGHLIGHT.g,
                                       Colors::OCCURRENCE_HIGHLIGHT.b, Colors::OCCURRENCE_HIGHLIGHT.a);
                SDL_Rect hl_rect = {hl_x_start, y, hl_w, line_height};
                SDL_RenderFillRect(renderer, &hl_rect);
            }
        }

        bool has_selection = sel_active && (sel_start_line != cursor_line || sel_start_col != cursor_col);
        if (has_selection) {
            LineIdx s_line = sel_start_line;
            ColIdx s_col = sel_start_col;
            LineIdx e_line = cursor_line;
            ColIdx e_col = cursor_col;
            if (s_line > e_line || (s_line == e_line && s_col > e_col)) {
                std::swap(s_line, e_line);
                std::swap(s_col, e_col);
            }

            if (i >= s_line && i <= e_line) {
                std::string expanded_line = expand_tabs(doc.lines[i]);
                int line_len = static_cast<int>(doc.lines[i].size());
                int line_start = (i == s_line) ? std::min(s_col, line_len) : 0;
                int line_end = (i == e_line) ? std::min(e_col, line_len) : line_len;
                int exp_start = expanded_column(doc.lines[i], line_start);
                int exp_end = expanded_column(doc.lines[i], line_end);
                int x_start = text_x;
                if (exp_start > 0) {
                    int w = 0;
                    TTF_SizeUTF8(font, expanded_line.substr(0, exp_start).c_str(), &w, nullptr);
                    x_start += w;
                }
                int sel_w = 0;
                if (exp_end > exp_start) {
                    TTF_SizeUTF8(font, expanded_line.substr(exp_start, exp_end - exp_start).c_str(), &sel_w, nullptr);
                }
                if (i < e_line) {
                    sel_w += char_width;
                }
                if (sel_w > 0) {
                    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                    SDL_SetRenderDrawColor(renderer, Colors::SELECTION.r, Colors::SELECTION.g, Colors::SELECTION.b, Colors::SELECTION.a);
                    SDL_Rect sel_rect = {x_start, y, sel_w, line_height};
                    SDL_RenderFillRect(renderer, &sel_rect);
                }
            }
        }

        if (!search_query.empty() && !doc.lines[i].empty()) {
            std::string expanded_line = expand_tabs(doc.lines[i]);
            size_t pos = 0;
            while ((pos = doc.lines[i].find(search_query, pos)) != std::string::npos) {
                int exp_pos = expanded_column(doc.lines[i], static_cast<int>(pos));
                int exp_end = expanded_column(doc.lines[i], static_cast<int>(pos + search_query.size()));
                int x_start = text_x;
                if (exp_pos > 0) {
                    int w = 0;
                    TTF_SizeUTF8(font, expanded_line.substr(0, exp_pos).c_str(), &w, nullptr);
                    x_start += w;
                }
                int highlight_w = 0;
                TTF_SizeUTF8(font, expanded_line.substr(exp_pos, exp_end - exp_pos).c_str(), &highlight_w, nullptr);
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(renderer, Colors::SEARCH_HIGHLIGHT.r, Colors::SEARCH_HIGHLIGHT.g,
                                       Colors::SEARCH_HIGHLIGHT.b, Colors::SEARCH_HIGHLIGHT.a);
                SDL_Rect highlight_rect = {x_start, y, highlight_w, line_height};
                SDL_RenderFillRect(renderer, &highlight_rect);
                pos += search_query.size();
            }
        }

        if (!doc.lines[i].empty()) {
            const std::string& line_text = doc.lines[i];
            const std::vector<Token>& tokens = const_cast<EditorView*>(this)->get_line_tokens(i);

            if (line_text.size() > LONG_LINE_THRESHOLD) {
                int effective_char_width = (char_width > 0) ? char_width : 10;
                int start_char_idx = std::max(0, scroll_x / effective_char_width);
                int start_byte = std::min(static_cast<int>(line_text.size()), start_char_idx);

                while (start_byte > 0 && (line_text[start_byte] & 0xC0) == 0x80) {
                    start_byte--;
                }

                int visible_chars_count = (window_w / effective_char_width) + 20;
                int len_bytes = visible_chars_count * 4;

                std::string sub_text = line_text.substr(start_byte, len_bytes);

                std::vector<Token> sub_tokens;
                int sub_len = static_cast<int>(sub_text.size());
                for (const auto& t : tokens) {
                    int new_start = t.start - start_byte;
                    int new_end = t.end - start_byte;

                    if (new_end <= 0 || new_start >= sub_len) continue;

                    sub_tokens.push_back({
                        t.type,
                        std::max(0, new_start),
                        std::min(sub_len, new_end)
                    });
                }

                SurfacePtr surf(texture_cache.render_line_to_surface(sub_text, sub_tokens, Colors::TEXT, syntax_color_func));
                if (surf) {
                    TexturePtr tex(SDL_CreateTextureFromSurface(renderer, surf.get()));
                    if (tex) {
                        int offset_x_local = start_char_idx * char_width;
                        SDL_Rect dst = {text_x + offset_x_local, y, surf->w, surf->h};
                        SDL_RenderCopy(renderer, tex.get(), nullptr, &dst);
                    }
                }
            } else {
                CachedLineRender& cached = build_line_render(
                    line_render_cache, i, line_text, tokens, renderer, font, line_height, Colors::TEXT, syntax_color_func
                );
                render_line(cached, renderer, text_x, y);
            }
        }

        if (is_fold_start_folded(i)) {
            int fold_end = get_fold_end_line(i);
            std::string fold_text = std::format(" ... ({} lines)", fold_end - i);
            int line_w = 0;
            if (!doc.lines[i].empty()) {
                std::string expanded_line = expand_tabs(doc.lines[i]);
                TTF_SizeUTF8(font, expanded_line.c_str(), &line_w, nullptr);
            }
            texture_cache.render_cached_text(fold_text, Colors::FOLD_INDICATOR, text_x + line_w, y);
        }

        if (i == cursor_line && cursor_visible && is_file_open && has_focus) {
            int cursor_x_local = text_x;
            if (cursor_col > 0 && !doc.lines[i].empty()) {
                std::string expanded_before = expand_tabs(doc.lines[i].substr(0, cursor_col));
                int w = 0;
                TTF_SizeUTF8(font, expanded_before.c_str(), &w, nullptr);
                cursor_x_local += w;
            }
            SDL_SetRenderDrawColor(renderer, Colors::CURSOR.r, Colors::CURSOR.g, Colors::CURSOR.b, 255);
            SDL_Rect cursor_rect = {cursor_x_local, y, 2, line_height};
            SDL_RenderFillRect(renderer, &cursor_rect);
        }

        y += line_height;
    }

    SDL_RenderSetClipRect(renderer, nullptr);

    int total_visible = get_total_visible_lines(doc);
    int visible_lines_count = visible_height / line_height;
    if (total_visible > visible_lines_count) {
        int scrollbar_x = x_offset + visible_width - scaled_scrollbar_width;

        SDL_SetRenderDrawColor(renderer, Colors::SCROLLBAR_BG.r, Colors::SCROLLBAR_BG.g, Colors::SCROLLBAR_BG.b, 255);
        SDL_Rect scrollbar_bg = {scrollbar_x, y_offset, scaled_scrollbar_width, visible_height};
        SDL_RenderFillRect(renderer, &scrollbar_bg);

        int thumb_height, thumb_y_pos;
        get_scrollbar_metrics(visible_height, scaled_scrollbar_min_thumb, thumb_height, thumb_y_pos, doc);

        SDL_Color thumb_color = scrollbar_dragging ? Colors::SCROLLBAR_THUMB_ACTIVE :
                                (scrollbar_hovered ? Colors::SCROLLBAR_THUMB_HOVER : Colors::SCROLLBAR_THUMB);
        SDL_SetRenderDrawColor(renderer, thumb_color.r, thumb_color.g, thumb_color.b, 255);

        int thumb_margin = layout.scaled(2);
        SDL_Rect thumb_rect = {scrollbar_x + thumb_margin, y_offset + thumb_y_pos,
                               scaled_scrollbar_width - thumb_margin * 2, thumb_height};
        SDL_RenderFillRect(renderer, &thumb_rect);
    }
}

void EditorView::clear_caches() {
    token_cache.clear();
    viewport_tokens_buffer.clear();
    line_render_cache.clear();
    highlight_occurrences.clear();
    highlight_occurrences.shrink_to_fit();
    highlighted_identifier.clear();
    last_highlight_line = -1;
    last_highlight_col = -1;
    fold_regions.clear();
    fold_regions.shrink_to_fit();
    folded_lines.clear();

    precise_scroll_x = 0.0;
    velocity_x = 0.0;
    scroll_x = 0;
    scroll_y = 0;
    precise_scroll_y = 0.0;
    target_scroll_y = 0.0;
    velocity_y = 0.0;
    scroll_state = ScrollState::Idle;
    last_update_time = SDL_GetTicks();
}
