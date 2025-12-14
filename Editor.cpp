#include "Editor.h"
#include "Utils.h"
#include "RenderUtils.h"
#include <fstream>
#include <algorithm>
#include <cstring>
#include <climits>

Editor::Editor() {
    lines.emplace_back("");
    rebuild_line_offsets();
}

void Editor::rebuild_line_offsets() {
    line_offsets.clear();
    line_offsets.reserve(lines.size() + 1);
    uint32_t offset = 0;
    for (const auto& line : lines) {
        line_offsets.push_back(offset);
        offset += static_cast<uint32_t>(line.size()) + 1;
    }
    line_offsets.push_back(offset);
}

void Editor::update_line_offsets(int start_line, int delta) {
    if (delta == 0) return;
    for (size_t i = start_line + 1; i < line_offsets.size(); ++i) {
        line_offsets[i] += delta;
    }
}

uint32_t Editor::get_byte_offset(int line, int col) const {
    if (line < 0 || line >= static_cast<int>(line_offsets.size()) - 1) return 0;
    return line_offsets[line] + static_cast<uint32_t>(col);
}

void Editor::perform_tree_edit(uint32_t start_byte, uint32_t bytes_removed, uint32_t bytes_added,
                               TSPoint start_point, TSPoint old_end_point, TSPoint new_end_point) {
    highlighter.apply_edit(
        start_byte,
        start_byte + bytes_removed,
        start_byte + bytes_added,
        start_point,
        old_end_point,
        new_end_point
    );
    syntax_dirty = true;
}

void Editor::mark_modified() {
    modified = true;
    syntax_dirty = true;
    last_edit_time = SDL_GetTicks();
    token_cache.clear();
}

void Editor::begin_undo_group() {
    if (!in_undo_group) {
        current_group_id++;
        in_undo_group = true;
    }
}

void Editor::end_undo_group() {
    in_undo_group = false;
}

uint64_t Editor::get_undo_group_id() {
    if (!in_undo_group) {
        current_group_id++;
    }
    return current_group_id;
}

void Editor::push_undo(EditOperation op) {
    if (undo_stack.size() >= UNDO_HISTORY_MAX) {
        undo_stack.erase(undo_stack.begin());
    }
    undo_stack.push_back(std::move(op));
    redo_stack.clear();
}

void Editor::apply_insert_internal(int line, int col, const std::string& text, int& out_end_line, int& out_end_col) {
    cursor_line = line;
    cursor_col = col;

    uint32_t start_byte = get_byte_offset(cursor_line, cursor_col);
    TSPoint start_point = {static_cast<uint32_t>(cursor_line), static_cast<uint32_t>(cursor_col)};
    TSPoint old_end_point = start_point;

    size_t pos = 0;
    while (pos < text.size()) {
        size_t newline = text.find('\n', pos);
        if (newline == std::string::npos) {
            lines[cursor_line].insert(cursor_col, text.substr(pos));
            cursor_col += static_cast<int>(text.size() - pos);
            break;
        }
        lines[cursor_line].insert(cursor_col, text.substr(pos, newline - pos));
        cursor_col += static_cast<int>(newline - pos);
        std::string remainder = lines[cursor_line].substr(cursor_col);
        lines[cursor_line] = lines[cursor_line].substr(0, cursor_col);
        cursor_line++;
        cursor_col = 0;
        lines.insert(lines.begin() + cursor_line, remainder);
        pos = newline + 1;
    }

    out_end_line = cursor_line;
    out_end_col = cursor_col;

    mark_modified();
    TSPoint new_end_point = {static_cast<uint32_t>(cursor_line), static_cast<uint32_t>(cursor_col)};
    perform_tree_edit(start_byte, 0, static_cast<uint32_t>(text.size()), start_point, old_end_point, new_end_point);
}

void Editor::apply_delete_internal(int start_line, int start_col, int end_line, int end_col, std::string& out_deleted) {
    int s_line = start_line, s_col = start_col;
    int e_line = end_line, e_col = end_col;

    if (s_line > e_line || (s_line == e_line && s_col > e_col)) {
        std::swap(s_line, e_line);
        std::swap(s_col, e_col);
    }

    for (int i = s_line; i <= e_line; i++) {
        int col_start = (i == s_line) ? s_col : 0;
        int col_end = (i == e_line) ? e_col : static_cast<int>(lines[i].size());
        out_deleted += lines[i].substr(col_start, col_end - col_start);
        if (i < e_line) out_deleted += '\n';
    }

    uint32_t start_byte = get_byte_offset(s_line, s_col);
    uint32_t end_byte = get_byte_offset(e_line, e_col);
    uint32_t bytes_removed = end_byte - start_byte;
    TSPoint start_point = {static_cast<uint32_t>(s_line), static_cast<uint32_t>(s_col)};
    TSPoint old_end_point = {static_cast<uint32_t>(e_line), static_cast<uint32_t>(e_col)};

    if (s_line == e_line) {
        lines[s_line].erase(s_col, e_col - s_col);
    } else {
        std::string new_line_content = lines[s_line].substr(0, s_col) + lines[e_line].substr(e_col);
        lines.erase(lines.begin() + s_line, lines.begin() + e_line + 1);
        lines.insert(lines.begin() + s_line, new_line_content);
    }

    cursor_line = s_line;
    cursor_col = s_col;

    mark_modified();
    perform_tree_edit(start_byte, bytes_removed, 0, start_point, old_end_point, start_point);
}

bool Editor::undo() {
    if (undo_stack.empty()) return false;

    uint64_t group = undo_stack.back().group_id;

    while (!undo_stack.empty() && undo_stack.back().group_id == group) {
        EditOperation op = std::move(undo_stack.back());
        undo_stack.pop_back();

        EditOperation reverse_op;
        reverse_op.group_id = op.group_id;

        if (op.type == EditOperationType::Insert) {
            reverse_op.type = EditOperationType::Delete;
            reverse_op.line = op.line;
            reverse_op.col = op.col;
            std::string deleted;
            apply_delete_internal(op.line, op.col, op.end_line, op.end_col, deleted);
            reverse_op.text = deleted;
            reverse_op.end_line = op.end_line;
            reverse_op.end_col = op.end_col;
        } else if (op.type == EditOperationType::MoveLine) {
            reverse_op.type = EditOperationType::MoveLine;
            int direction = op.col;
            int block_start = op.line;
            int block_end = op.end_line;

            int new_block_start = block_start + direction;
            int new_block_end = block_end + direction;

            int affected_start = std::min(block_start, new_block_start);
            int affected_end = std::max(block_end, new_block_end);

            uint32_t start_byte = line_offsets[affected_start];
            uint32_t end_byte = line_offsets[affected_end + 1];
            uint32_t byte_len = end_byte - start_byte;
            TSPoint start_point = {static_cast<uint32_t>(affected_start), 0};
            TSPoint end_point = {static_cast<uint32_t>(affected_end + 1), 0};

            if (direction == -1) {
                std::string moving_line = std::move(lines[new_block_end + 1]);
                for (int i = new_block_end + 1; i > new_block_start; --i) {
                    lines[i] = std::move(lines[i - 1]);
                }
                lines[new_block_start] = std::move(moving_line);
            } else {
                std::string moving_line = std::move(lines[new_block_start - 1]);
                for (int i = new_block_start - 1; i < new_block_end; ++i) {
                    lines[i] = std::move(lines[i + 1]);
                }
                lines[new_block_end] = std::move(moving_line);
            }

            cursor_line = block_start;
            reverse_op.line = new_block_start;
            reverse_op.col = -direction;
            reverse_op.end_line = new_block_end;
            reverse_op.end_col = 0;
            rebuild_line_offsets();
            perform_tree_edit(start_byte, byte_len, byte_len, start_point, end_point, end_point);
        } else {
            reverse_op.type = EditOperationType::Insert;
            reverse_op.line = op.line;
            reverse_op.col = op.col;
            reverse_op.text = op.text;
            int out_end_line, out_end_col;
            apply_insert_internal(op.line, op.col, op.text, out_end_line, out_end_col);
            reverse_op.end_line = out_end_line;
            reverse_op.end_col = out_end_col;
        }

        redo_stack.push_back(std::move(reverse_op));
    }

    clear_selection();
    return true;
}

bool Editor::redo() {
    if (redo_stack.empty()) return false;

    uint64_t group = redo_stack.back().group_id;

    while (!redo_stack.empty() && redo_stack.back().group_id == group) {
        EditOperation op = std::move(redo_stack.back());
        redo_stack.pop_back();

        EditOperation reverse_op;
        reverse_op.group_id = op.group_id;

        if (op.type == EditOperationType::Insert) {
            reverse_op.type = EditOperationType::Delete;
            reverse_op.line = op.line;
            reverse_op.col = op.col;
            std::string deleted;
            apply_delete_internal(op.line, op.col, op.end_line, op.end_col, deleted);
            reverse_op.text = deleted;
            reverse_op.end_line = op.end_line;
            reverse_op.end_col = op.end_col;
        } else if (op.type == EditOperationType::MoveLine) {
            reverse_op.type = EditOperationType::MoveLine;
            int direction = op.col;
            int block_start = op.line;
            int block_end = op.end_line;

            int new_block_start = block_start + direction;
            int new_block_end = block_end + direction;

            int affected_start = std::min(block_start, new_block_start);
            int affected_end = std::max(block_end, new_block_end);

            uint32_t start_byte = line_offsets[affected_start];
            uint32_t end_byte = line_offsets[affected_end + 1];
            uint32_t byte_len = end_byte - start_byte;
            TSPoint start_point = {static_cast<uint32_t>(affected_start), 0};
            TSPoint end_point = {static_cast<uint32_t>(affected_end + 1), 0};

            if (direction == -1) {
                std::string moving_line = std::move(lines[block_start - 1]);
                for (int i = block_start - 1; i < block_end; ++i) {
                    lines[i] = std::move(lines[i + 1]);
                }
                lines[block_end] = std::move(moving_line);
            } else {
                std::string moving_line = std::move(lines[block_end + 1]);
                for (int i = block_end + 1; i > block_start; --i) {
                    lines[i] = std::move(lines[i - 1]);
                }
                lines[block_start] = std::move(moving_line);
            }

            cursor_line = new_block_start;
            reverse_op.line = new_block_start;
            reverse_op.col = -direction;
            reverse_op.end_line = new_block_end;
            reverse_op.end_col = 0;
            rebuild_line_offsets();
            perform_tree_edit(start_byte, byte_len, byte_len, start_point, end_point, end_point);
        } else {
            reverse_op.type = EditOperationType::Insert;
            reverse_op.line = op.line;
            reverse_op.col = op.col;
            reverse_op.text = op.text;
            int out_end_line, out_end_col;
            apply_insert_internal(op.line, op.col, op.text, out_end_line, out_end_col);
            reverse_op.end_line = out_end_line;
            reverse_op.end_col = out_end_col;
        }

        undo_stack.push_back(std::move(reverse_op));
    }

    clear_selection();
    return true;
}

void Editor::rebuild_syntax() {
    if (!syntax_dirty) return;

    rebuild_line_offsets();
    highlighter.parse_incremental(lines, line_offsets);

    token_cache.clear();

    if (lines.size() < MAX_LINES_FOR_FOLDING) {
        update_fold_regions();
    } else {
        fold_regions.clear();
        folded_lines.clear();
    }

    syntax_dirty = false;
}

void Editor::prefetch_viewport_tokens(int start_line, int visible_count) {
    start_line = std::max(0, start_line);

    int lines_found = 0;
    int current_line = start_line;
    int max_lines = static_cast<int>(lines.size());

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
        highlighter.get_viewport_tokens(start_line, end_line, line_offsets, lines, viewport_tokens_buffer);
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

const std::vector<Token>& Editor::get_line_tokens(size_t line_idx) {
    static const std::vector<Token> empty_tokens;
    auto it = token_cache.find(line_idx);
    if (it != token_cache.end()) {
        return it->second;
    }
    return empty_tokens;
}

bool Editor::has_selection() const {
    return sel_active && (sel_start_line != cursor_line || sel_start_col != cursor_col);
}

void Editor::clear_selection() {
    sel_active = false;
}

void Editor::start_selection() {
    if (!sel_active) {
        sel_start_line = cursor_line;
        sel_start_col = cursor_col;
        sel_active = true;
    }
}

void Editor::get_selection_bounds(int& start_line, int& start_col, int& end_line, int& end_col) const {
    if (cursor_line < sel_start_line || (cursor_line == sel_start_line && cursor_col < sel_start_col)) {
        start_line = cursor_line;
        start_col = cursor_col;
        end_line = sel_start_line;
        end_col = sel_start_col;
    } else {
        start_line = sel_start_line;
        start_col = sel_start_col;
        end_line = cursor_line;
        end_col = cursor_col;
    }
}

std::string Editor::get_selected_text() const {
    if (!has_selection()) return "";
    int start_line, start_col, end_line, end_col;
    get_selection_bounds(start_line, start_col, end_line, end_col);
    std::string result;
    for (int i = start_line; i <= end_line; i++) {
        int col_start = (i == start_line) ? start_col : 0;
        int col_end = (i == end_line) ? end_col : static_cast<int>(lines[i].size());
        result += lines[i].substr(col_start, col_end - col_start);
        if (i < end_line) result += '\n';
    }
    return result;
}

void Editor::delete_selection() {
    if (readonly) return;
    if (!has_selection()) return;
    int start_line, start_col, end_line, end_col;
    get_selection_bounds(start_line, start_col, end_line, end_col);

    std::string deleted_text = get_selected_text();
    uint64_t group = get_undo_group_id();

    uint32_t start_byte = get_byte_offset(start_line, start_col);
    uint32_t end_byte = get_byte_offset(end_line, end_col);
    uint32_t bytes_removed = end_byte - start_byte;
    TSPoint start_point = {static_cast<uint32_t>(start_line), static_cast<uint32_t>(start_col)};
    TSPoint old_end_point = {static_cast<uint32_t>(end_line), static_cast<uint32_t>(end_col)};

    if (start_line == end_line) {
        lines[start_line].erase(start_col, end_col - start_col);
    } else {
        std::string new_line_content = lines[start_line].substr(0, start_col) + lines[end_line].substr(end_col);
        lines.erase(lines.begin() + start_line, lines.begin() + end_line + 1);
        lines.insert(lines.begin() + start_line, new_line_content);
    }

    cursor_line = start_line;
    cursor_col = start_col;
    clear_selection();
    mark_modified();
    perform_tree_edit(start_byte, bytes_removed, 0, start_point, old_end_point, start_point);

    EditOperation op;
    op.type = EditOperationType::Delete;
    op.line = start_line;
    op.col = start_col;
    op.text = deleted_text;
    op.end_line = end_line;
    op.end_col = end_col;
    op.group_id = group;
    push_undo(std::move(op));
}

void Editor::move_page_up(int visible_lines) {
    cursor_line = std::max(0, cursor_line - visible_lines);
    cursor_col = std::min(cursor_col, static_cast<int>(lines[cursor_line].size()));
    while (cursor_col > 0 && (lines[cursor_line][cursor_col] & 0xC0) == 0x80) {
        cursor_col--;
    }
}

void Editor::move_page_down(int visible_lines) {
    cursor_line = std::min(static_cast<int>(lines.size()) - 1, cursor_line + visible_lines);
    cursor_col = std::min(cursor_col, static_cast<int>(lines[cursor_line].size()));
    while (cursor_col > 0 && (lines[cursor_line][cursor_col] & 0xC0) == 0x80) {
        cursor_col--;
    }
}

void Editor::go_to(int line, int col) {
    cursor_line = std::max(0, std::min(line - 1, static_cast<int>(lines.size()) - 1));
    cursor_col = std::max(0, std::min(col > 0 ? col - 1 : 0, static_cast<int>(lines[cursor_line].size())));
    while (cursor_col > 0 && (lines[cursor_line][cursor_col] & 0xC0) == 0x80) {
        cursor_col--;
    }
    clear_selection();
}

bool Editor::save_file() {
    if (file_path.empty()) {
        file_path = show_save_dialog();
        if (file_path.empty()) {
            return false;
        }
    }
    std::ofstream file(file_path);
    if (!file.is_open()) {
        return false;
    }
    for (size_t i = 0; i < lines.size(); i++) {
        file << lines[i];
        if (i < lines.size() - 1) {
            file << '\n';
        }
    }
    modified = false;
    return true;
}

char Editor::get_closing_pair(char c) {
    switch (c) {
        case '(': return ')';
        case '[': return ']';
        case '{': return '}';
        case '"': return '"';
        case '\'': return '\'';
        default: return '\0';
    }
}

bool Editor::is_closing_char(char c) {
    return c == ')' || c == ']' || c == '}' || c == '"' || c == '\'';
}

char Editor::get_opening_pair(char c) {
    switch (c) {
        case ')': return '(';
        case ']': return '[';
        case '}': return '{';
        case '"': return '"';
        case '\'': return '\'';
        default: return '\0';
    }
}

void Editor::insert_text(const char* text) {
    if (readonly) return;
    if (has_selection()) {
        delete_selection();
    }

    std::string str(text);
    if (str.empty()) return;

    if (str.size() == 1) {
        char ch = str[0];
        const std::string& current_line = lines[cursor_line];

        if (is_closing_char(ch)) {
            if (cursor_col < static_cast<int>(current_line.size()) && current_line[cursor_col] == ch) {
                cursor_col++;
                return;
            }
        }

        char closing = get_closing_pair(ch);
        if (closing != '\0') {
            str += closing;
        }
    }

    int start_line_pos = cursor_line;
    int start_col_pos = cursor_col;
    uint64_t group = get_undo_group_id();

    uint32_t start_byte = get_byte_offset(cursor_line, cursor_col);
    TSPoint start_point = {static_cast<uint32_t>(cursor_line), static_cast<uint32_t>(cursor_col)};
    TSPoint old_end_point = start_point;

    int final_cursor_offset = 0;
    if (str.size() == 2 && get_closing_pair(str[0]) == str[1]) {
        final_cursor_offset = 1;
    }

    bool has_newlines = str.find('\n') != std::string::npos;

    size_t pos = 0;
    while (pos < str.size()) {
        size_t newline = str.find('\n', pos);
        if (newline == std::string::npos) {
            lines[cursor_line].insert(cursor_col, str.substr(pos));
            cursor_col += static_cast<int>(str.size() - pos);
            break;
        }
        lines[cursor_line].insert(cursor_col, str.substr(pos, newline - pos));
        cursor_col += static_cast<int>(newline - pos);
        std::string remainder = lines[cursor_line].substr(cursor_col);
        lines[cursor_line] = lines[cursor_line].substr(0, cursor_col);
        cursor_line++;
        cursor_col = 0;
        lines.insert(lines.begin() + cursor_line, remainder);
        pos = newline + 1;
    }

    cursor_col -= final_cursor_offset;

    mark_modified();

    if (has_newlines) {
        rebuild_line_offsets();
    } else {
        update_line_offsets(start_line_pos, static_cast<int>(str.size()));
    }

    TSPoint new_end_point = {static_cast<uint32_t>(cursor_line), static_cast<uint32_t>(cursor_col + final_cursor_offset)};
    perform_tree_edit(start_byte, 0, static_cast<uint32_t>(str.size()), start_point, old_end_point, new_end_point);

    EditOperation op;
    op.type = EditOperationType::Insert;
    op.line = start_line_pos;
    op.col = start_col_pos;
    op.text = str;
    op.end_line = cursor_line;
    op.end_col = cursor_col + final_cursor_offset;
    op.group_id = group;
    push_undo(std::move(op));
}

void Editor::new_line() {
    if (readonly) return;
    std::string indent;
    const std::string& current_text = lines[cursor_line];
    for (char c : current_text) {
        if (c == ' ' || c == '\t') {
            indent += c;
        } else {
            break;
        }
    }

    if (cursor_col > 0 && current_text[cursor_col - 1] == '{') {
        indent += "    ";
    }

    int start_line_pos = cursor_line;
    int start_col_pos = cursor_col;
    uint64_t group = get_undo_group_id();

    uint32_t start_byte = get_byte_offset(cursor_line, cursor_col);
    TSPoint start_point = {static_cast<uint32_t>(cursor_line), static_cast<uint32_t>(cursor_col)};
    TSPoint old_end_point = start_point;

    std::string remainder = lines[cursor_line].substr(cursor_col);
    lines[cursor_line] = lines[cursor_line].substr(0, cursor_col);

    cursor_line++;

    remainder = indent + remainder;
    cursor_col = static_cast<int>(indent.size());

    lines.insert(lines.begin() + cursor_line, remainder);

    mark_modified();

    uint32_t bytes_added = 1 + static_cast<uint32_t>(indent.size());

    TSPoint new_end_point = {static_cast<uint32_t>(cursor_line), static_cast<uint32_t>(cursor_col)};
    perform_tree_edit(start_byte, 0, bytes_added, start_point, old_end_point, new_end_point);

    EditOperation op;
    op.type = EditOperationType::Insert;
    op.line = start_line_pos;
    op.col = start_col_pos;
    op.text = "\n" + indent;
    op.end_line = cursor_line;
    op.end_col = cursor_col;
    op.group_id = group;
    push_undo(std::move(op));
}

void Editor::backspace() {
    if (readonly) return;
    if (has_selection()) {
        delete_selection();
        return;
    }
    if (cursor_col > 0) {
        int prev_pos = utf8_prev_char_start(lines[cursor_line], cursor_col);
        const std::string& current_line = lines[cursor_line];

        bool delete_pair = false;
        if (cursor_col < static_cast<int>(current_line.size())) {
            char left_char = current_line[prev_pos];
            char right_char = current_line[cursor_col];
            if (get_closing_pair(left_char) == right_char && right_char != '\0') {
                delete_pair = true;
            }
        }

        int delete_end = delete_pair ? cursor_col + 1 : cursor_col;
        std::string deleted_text = current_line.substr(prev_pos, delete_end - prev_pos);
        int orig_col = delete_end;
        uint64_t group = get_undo_group_id();

        uint32_t bytes_removed = static_cast<uint32_t>(delete_end - prev_pos);
        TSPoint old_end_point = {static_cast<uint32_t>(cursor_line), static_cast<uint32_t>(delete_end)};

        lines[cursor_line].erase(prev_pos, delete_end - prev_pos);
        cursor_col = prev_pos;

        mark_modified();
        update_line_offsets(cursor_line, -static_cast<int>(bytes_removed));

        uint32_t start_byte = get_byte_offset(cursor_line, cursor_col);
        TSPoint start_point = {static_cast<uint32_t>(cursor_line), static_cast<uint32_t>(cursor_col)};
        perform_tree_edit(start_byte, bytes_removed, 0, start_point, old_end_point, start_point);

        EditOperation op;
        op.type = EditOperationType::Delete;
        op.line = cursor_line;
        op.col = prev_pos;
        op.text = deleted_text;
        op.end_line = cursor_line;
        op.end_col = orig_col;
        op.group_id = group;
        push_undo(std::move(op));
    } else if (cursor_line > 0) {
        int orig_line = cursor_line;
        uint64_t group = get_undo_group_id();

        TSPoint old_end_point = {static_cast<uint32_t>(cursor_line), 0};
        int new_col = static_cast<int>(lines[cursor_line - 1].size());

        lines[cursor_line - 1] += lines[cursor_line];
        lines.erase(lines.begin() + cursor_line);
        cursor_line--;
        cursor_col = new_col;

        mark_modified();
        rebuild_line_offsets();

        uint32_t start_byte = get_byte_offset(cursor_line, cursor_col);
        TSPoint start_point = {static_cast<uint32_t>(cursor_line), static_cast<uint32_t>(cursor_col)};
        perform_tree_edit(start_byte, 1, 0, start_point, old_end_point, start_point);

        EditOperation op;
        op.type = EditOperationType::Delete;
        op.line = cursor_line;
        op.col = new_col;
        op.text = "\n";
        op.end_line = orig_line;
        op.end_col = 0;
        op.group_id = group;
        push_undo(std::move(op));
    }
}

void Editor::delete_char() {
    if (readonly) return;
    if (has_selection()) {
        delete_selection();
        return;
    }
    if (cursor_col < static_cast<int>(lines[cursor_line].size())) {
        int next_pos = utf8_next_char_pos(lines[cursor_line], cursor_col);
        std::string deleted_text = lines[cursor_line].substr(cursor_col, next_pos - cursor_col);
        uint64_t group = get_undo_group_id();

        uint32_t bytes_removed = static_cast<uint32_t>(next_pos - cursor_col);
        uint32_t start_byte = get_byte_offset(cursor_line, cursor_col);
        TSPoint start_point = {static_cast<uint32_t>(cursor_line), static_cast<uint32_t>(cursor_col)};
        TSPoint old_end_point = {static_cast<uint32_t>(cursor_line), static_cast<uint32_t>(next_pos)};

        lines[cursor_line].erase(cursor_col, next_pos - cursor_col);

        mark_modified();
        perform_tree_edit(start_byte, bytes_removed, 0, start_point, old_end_point, start_point);

        EditOperation op;
        op.type = EditOperationType::Delete;
        op.line = cursor_line;
        op.col = cursor_col;
        op.text = deleted_text;
        op.end_line = cursor_line;
        op.end_col = next_pos;
        op.group_id = group;
        push_undo(std::move(op));
    } else if (cursor_line < static_cast<int>(lines.size()) - 1) {
        uint64_t group = get_undo_group_id();

        uint32_t start_byte = get_byte_offset(cursor_line, cursor_col);
        TSPoint start_point = {static_cast<uint32_t>(cursor_line), static_cast<uint32_t>(cursor_col)};
        TSPoint old_end_point = {static_cast<uint32_t>(cursor_line + 1), static_cast<uint32_t>(lines[cursor_line + 1].size())};

        lines[cursor_line] += lines[cursor_line + 1];
        lines.erase(lines.begin() + cursor_line + 1);

        mark_modified();
        perform_tree_edit(start_byte, 1, 0, start_point, old_end_point, start_point);

        EditOperation op;
        op.type = EditOperationType::Delete;
        op.line = cursor_line;
        op.col = cursor_col;
        op.text = "\n";
        op.end_line = cursor_line + 1;
        op.end_col = 0;
        op.group_id = group;
        push_undo(std::move(op));
    }
}

void Editor::toggle_comment() {
    if (readonly) return;
    const std::string& comment_token = highlighter.get_line_comment_token();
    if (comment_token.empty()) return;

    int start_line = cursor_line;
    int end_line = cursor_line;

    if (has_selection()) {
        int s_line, s_col, e_line, e_col;
        get_selection_bounds(s_line, s_col, e_line, e_col);
        start_line = s_line;
        end_line = e_line;
    }

    bool all_commented = true;
    for (int i = start_line; i <= end_line; ++i) {
        const std::string& line = lines[i];
        size_t first_non_space = line.find_first_not_of(" \t");
        if (first_non_space == std::string::npos) continue;
        if (line.substr(first_non_space, comment_token.size()) != comment_token) {
            all_commented = false;
            break;
        }
    }

    begin_undo_group();

    int min_indent = INT_MAX;
    if (!all_commented) {
        for (int i = start_line; i <= end_line; ++i) {
            const std::string& line = lines[i];
            size_t first_non_space = line.find_first_not_of(" \t");
            if (first_non_space == std::string::npos) continue;
            min_indent = std::min(min_indent, static_cast<int>(first_non_space));
        }
        if (min_indent == INT_MAX) min_indent = 0;
    }

    for (int i = start_line; i <= end_line; ++i) {
        std::string& line = lines[i];
        size_t first_non_space = line.find_first_not_of(" \t");

        if (all_commented) {
            if (first_non_space != std::string::npos &&
                line.substr(first_non_space, comment_token.size()) == comment_token) {

                int del_start = static_cast<int>(first_non_space);
                int del_len = static_cast<int>(comment_token.size());
                if (first_non_space + comment_token.size() < line.size() &&
                    line[first_non_space + comment_token.size()] == ' ') {
                    del_len++;
                }

                std::string deleted = line.substr(del_start, del_len);

                uint32_t start_byte = get_byte_offset(i, del_start);
                TSPoint start_point = {static_cast<uint32_t>(i), static_cast<uint32_t>(del_start)};
                TSPoint old_end_point = {static_cast<uint32_t>(i), static_cast<uint32_t>(del_start + del_len)};

                line.erase(del_start, del_len);

                perform_tree_edit(start_byte, del_len, 0, start_point, old_end_point, start_point);

                EditOperation op;
                op.type = EditOperationType::Delete;
                op.line = i;
                op.col = del_start;
                op.text = deleted;
                op.end_line = i;
                op.end_col = del_start + del_len;
                op.group_id = current_group_id;
                push_undo(std::move(op));
            }
        } else {
            std::string insert_str = comment_token + " ";
            int insert_pos = min_indent;

            uint32_t start_byte = get_byte_offset(i, insert_pos);
            TSPoint start_point = {static_cast<uint32_t>(i), static_cast<uint32_t>(insert_pos)};

            line.insert(insert_pos, insert_str);

            TSPoint new_end_point = {static_cast<uint32_t>(i), static_cast<uint32_t>(insert_pos + insert_str.size())};
            perform_tree_edit(start_byte, 0, static_cast<uint32_t>(insert_str.size()), start_point, start_point, new_end_point);

            EditOperation op;
            op.type = EditOperationType::Insert;
            op.line = i;
            op.col = insert_pos;
            op.text = insert_str;
            op.end_line = i;
            op.end_col = insert_pos + static_cast<int>(insert_str.size());
            op.group_id = current_group_id;
            push_undo(std::move(op));
        }
    }

    end_undo_group();
    mark_modified();
    rebuild_line_offsets();
}

void Editor::move_left() {
    if (cursor_col > 0) {
        cursor_col = utf8_prev_char_start(lines[cursor_line], cursor_col);
    }
}

void Editor::move_right() {
    if (cursor_col < static_cast<int>(lines[cursor_line].size())) {
        cursor_col = utf8_next_char_pos(lines[cursor_line], cursor_col);
    }
}

bool Editor::is_word_char_at(const std::string& str, int pos) const {
    return is_word_codepoint(utf8_decode_at(str, pos));
}

void Editor::move_word_left() {
    const std::string& line = lines[cursor_line];

    if (cursor_col == 0) {
        if (cursor_line > 0) {
            cursor_line--;
            cursor_col = static_cast<int>(lines[cursor_line].size());
        }
        return;
    }

    cursor_col = utf8_prev_char_start(line, cursor_col);

    while (cursor_col > 0 && !is_word_char_at(line, cursor_col)) {
        cursor_col = utf8_prev_char_start(line, cursor_col);
    }

    while (cursor_col > 0) {
        int prev = utf8_prev_char_start(line, cursor_col);
        if (!is_word_char_at(line, prev)) break;
        cursor_col = prev;
    }
}

void Editor::move_word_right() {
    const std::string& line = lines[cursor_line];
    int line_len = static_cast<int>(line.size());

    if (cursor_col >= line_len) {
        if (cursor_line < static_cast<int>(lines.size()) - 1) {
            cursor_line++;
            cursor_col = 0;
        }
        return;
    }

    while (cursor_col < line_len && is_word_char_at(line, cursor_col)) {
        cursor_col = utf8_next_char_pos(line, cursor_col);
    }

    while (cursor_col < line_len && !is_word_char_at(line, cursor_col)) {
        cursor_col = utf8_next_char_pos(line, cursor_col);
    }
}

void Editor::delete_word_left() {
    if (readonly) return;
    if (has_selection()) {
        delete_selection();
        return;
    }

    int orig_line = cursor_line;
    int orig_col = cursor_col;

    move_word_left();

    if (cursor_line != orig_line || cursor_col != orig_col) {
        sel_start_line = orig_line;
        sel_start_col = orig_col;
        sel_active = true;
        delete_selection();
    }
}

void Editor::delete_word_right() {
    if (readonly) return;
    if (has_selection()) {
        delete_selection();
        return;
    }

    int orig_line = cursor_line;
    int orig_col = cursor_col;

    move_word_right();

    if (cursor_line != orig_line || cursor_col != orig_col) {
        sel_start_line = orig_line;
        sel_start_col = orig_col;
        sel_active = true;
        delete_selection();
    }
}

void Editor::move_up() {
    if (cursor_line > 0) {
        int new_line = get_next_visible_line(cursor_line, -1);
        if (new_line != cursor_line) {
            cursor_line = new_line;
            cursor_col = std::min(cursor_col, static_cast<int>(lines[cursor_line].size()));
            while (cursor_col > 0 && (lines[cursor_line][cursor_col] & 0xC0) == 0x80) {
                cursor_col--;
            }
        }
    }
}

void Editor::move_down() {
    if (cursor_line < static_cast<int>(lines.size()) - 1) {
        int new_line = get_next_visible_line(cursor_line, 1);
        if (new_line != cursor_line) {
            cursor_line = new_line;
            cursor_col = std::min(cursor_col, static_cast<int>(lines[cursor_line].size()));
            while (cursor_col > 0 && (lines[cursor_line][cursor_col] & 0xC0) == 0x80) {
                cursor_col--;
            }
        }
    }
}

void Editor::move_line_up() {
    if (readonly) return;
    int block_start, block_end;

    if (has_selection()) {
        int s_line, s_col, e_line, e_col;
        get_selection_bounds(s_line, s_col, e_line, e_col);
        block_start = s_line;
        block_end = e_line;
    } else {
        block_start = cursor_line;
        block_end = cursor_line;
    }

    if (block_start <= 0) return;

    int affected_start = block_start - 1;
    int affected_end = block_end;

    uint32_t start_byte = line_offsets[affected_start];
    uint32_t end_byte = line_offsets[affected_end + 1];
    uint32_t byte_len = end_byte - start_byte;

    TSPoint start_point = {static_cast<uint32_t>(affected_start), 0};
    TSPoint end_point = {static_cast<uint32_t>(affected_end + 1), 0};

    uint64_t group = get_undo_group_id();

    std::string moving_line = std::move(lines[block_start - 1]);
    for (int i = block_start - 1; i < block_end; ++i) {
        lines[i] = std::move(lines[i + 1]);
    }
    lines[block_end] = std::move(moving_line);

    cursor_line--;
    if (has_selection()) {
        sel_start_line--;
    }

    mark_modified();
    rebuild_line_offsets();

    perform_tree_edit(start_byte, byte_len, byte_len, start_point, end_point, end_point);

    EditOperation op;
    op.type = EditOperationType::MoveLine;
    op.line = block_start;
    op.col = -1;
    op.end_line = block_end;
    op.end_col = 0;
    op.group_id = group;
    push_undo(std::move(op));
}

void Editor::move_line_down() {
    if (readonly) return;
    int block_start, block_end;

    if (has_selection()) {
        int s_line, s_col, e_line, e_col;
        get_selection_bounds(s_line, s_col, e_line, e_col);
        block_start = s_line;
        block_end = e_line;
    } else {
        block_start = cursor_line;
        block_end = cursor_line;
    }

    if (block_end >= static_cast<int>(lines.size()) - 1) return;

    int affected_start = block_start;
    int affected_end = block_end + 1;

    uint32_t start_byte = line_offsets[affected_start];
    uint32_t end_byte = line_offsets[affected_end + 1];
    uint32_t byte_len = end_byte - start_byte;

    TSPoint start_point = {static_cast<uint32_t>(affected_start), 0};
    TSPoint end_point = {static_cast<uint32_t>(affected_end + 1), 0};

    uint64_t group = get_undo_group_id();

    std::string moving_line = std::move(lines[block_end + 1]);
    for (int i = block_end + 1; i > block_start; --i) {
        lines[i] = std::move(lines[i - 1]);
    }
    lines[block_start] = std::move(moving_line);

    cursor_line++;
    if (has_selection()) {
        sel_start_line++;
    }

    mark_modified();
    rebuild_line_offsets();

    perform_tree_edit(start_byte, byte_len, byte_len, start_point, end_point, end_point);

    EditOperation op;
    op.type = EditOperationType::MoveLine;
    op.line = block_start;
    op.col = 1;
    op.end_line = block_end;
    op.end_col = 0;
    op.group_id = group;
    push_undo(std::move(op));
}

void Editor::move_home() {
    cursor_col = 0;
}

void Editor::move_end() {
    cursor_col = static_cast<int>(lines[cursor_line].size());
}

void Editor::duplicate_line() {
    if (readonly) return;
    if (has_selection()) {
        std::string selected = get_selected_text();
        int start_line, start_col, end_line, end_col;
        get_selection_bounds(start_line, start_col, end_line, end_col);
        cursor_line = end_line;
        cursor_col = end_col;
        clear_selection();
        insert_text(selected.c_str());
    } else {
        std::string current = lines[cursor_line];
        int orig_line = cursor_line;
        uint64_t group = get_undo_group_id();

        uint32_t start_byte = get_byte_offset(cursor_line, static_cast<int>(current.size()));
        TSPoint start_point = {static_cast<uint32_t>(cursor_line), static_cast<uint32_t>(current.size())};
        TSPoint old_end_point = start_point;

        lines.insert(lines.begin() + cursor_line + 1, current);
        cursor_line++;

        mark_modified();
        uint32_t bytes_added = static_cast<uint32_t>(current.size()) + 1;
        TSPoint new_end_point = {static_cast<uint32_t>(cursor_line), static_cast<uint32_t>(current.size())};
        perform_tree_edit(start_byte, 0, bytes_added, start_point, old_end_point, new_end_point);

        EditOperation op;
        op.type = EditOperationType::Insert;
        op.line = orig_line;
        op.col = static_cast<int>(current.size());
        op.text = "\n" + current;
        op.end_line = cursor_line;
        op.end_col = static_cast<int>(current.size());
        op.group_id = group;
        push_undo(std::move(op));
    }
}

bool Editor::find_next(const std::string& query, int start_line, int start_col) {
    if (query.empty()) return false;
    clear_selection();
    for (int i = start_line; i < static_cast<int>(lines.size()); i++) {
        size_t search_start = (i == start_line) ? start_col : 0;
        size_t pos = lines[i].find(query, search_start);
        if (pos != std::string::npos) {
            cursor_line = i;
            cursor_col = static_cast<int>(pos);
            return true;
        }
    }
    for (int i = 0; i <= start_line; i++) {
        size_t end_col = (i == start_line) ? start_col : lines[i].size();
        size_t pos = lines[i].find(query);
        if (pos != std::string::npos && pos < end_col) {
            cursor_line = i;
            cursor_col = static_cast<int>(pos);
            return true;
        }
    }
    return false;
}

int Editor::count_visible_lines_between(int from_line, int to_line) const {
    int count = 0;
    int start = std::min(from_line, to_line);
    int end = std::max(from_line, to_line);
    for (int i = start; i <= end; i++) {
        if (!is_line_folded(i)) {
            count++;
        }
    }
    return count;
}

int Editor::get_nth_visible_line_from(int start_line, int n) const {
    int count = 0;
    int line = start_line;
    int direction = (n >= 0) ? 1 : -1;
    int target = std::abs(n);
    while (line >= 0 && line < static_cast<int>(lines.size())) {
        if (!is_line_folded(line)) {
            if (count == target) return line;
            count++;
        }
        line += direction;
    }
    return std::clamp(line - direction, 0, static_cast<int>(lines.size()) - 1);
}

int Editor::get_first_visible_line_from(int line) const {
    while (line > 0 && is_line_folded(line)) {
        line--;
    }
    return line;
}

void Editor::ensure_visible(int visible_lines) {
    int cursor_visible = get_first_visible_line_from(cursor_line);
    if (cursor_visible < scroll_y) {
        scroll_y = cursor_visible;
    }
    int visible_from_scroll = count_visible_lines_between(scroll_y, cursor_visible);
    if (visible_from_scroll > visible_lines) {
        int lines_to_skip = visible_from_scroll - visible_lines;
        scroll_y = get_nth_visible_line_from(scroll_y, lines_to_skip);
    }
    scroll_y = get_first_visible_line_from(scroll_y);
}

void Editor::ensure_visible_x(int cursor_pixel_x, int visible_width, int margin) {
    if (cursor_pixel_x - scroll_x < margin) {
        scroll_x = std::max(0, cursor_pixel_x - margin);
    }
    if (cursor_pixel_x - scroll_x > visible_width - margin) {
        scroll_x = cursor_pixel_x - visible_width + margin;
    }
}

bool Editor::load_file(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return false;

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (fsize < 0) { fclose(f); return false; }

    std::string buffer;
    buffer.resize(fsize);
    size_t read_size = fread(buffer.data(), 1, fsize, f);
    fclose(f);
    buffer.resize(read_size);

    lines.clear();
    lines.shrink_to_fit();
    token_cache.clear();
    viewport_tokens_buffer.clear();
    line_offsets.clear();
    line_offsets.shrink_to_fit();

    line_offsets.reserve(fsize / 40);

    lines.reserve(fsize / 40);

    const char* ptr = buffer.data();
    const char* end = ptr + buffer.size();
    const char* line_start = ptr;

    uint32_t current_offset = 0;

    while (ptr < end) {
        if (*ptr == '\n') {
            lines.emplace_back(line_start, ptr - line_start);
            line_offsets.push_back(current_offset);
            current_offset += static_cast<uint32_t>(ptr - line_start) + 1;
            line_start = ptr + 1;
        }
        ptr++;
    }

    if (line_start < end) {
        lines.emplace_back(line_start, end - line_start);
        line_offsets.push_back(current_offset);
        current_offset += static_cast<uint32_t>(end - line_start);
    } else {
        lines.emplace_back("");
        line_offsets.push_back(current_offset);
    }

    line_offsets.push_back(current_offset);

    if (lines.empty()) {
        lines.push_back("");
    }

    file_path = path;
    cursor_line = 0;
    cursor_col = 0;
    scroll_y = 0;
    scroll_x = 0;
    modified = false;
    syntax_dirty = true;

    undo_stack.clear();
    undo_stack.shrink_to_fit();
    redo_stack.clear();
    redo_stack.shrink_to_fit();
    current_group_id = 0;
    in_undo_group = false;

    highlight_occurrences.clear();
    highlight_occurrences.shrink_to_fit();
    highlighted_identifier.clear();
    last_highlight_line = -1;
    last_highlight_col = -1;

    selection_stack.clear();
    selection_stack.shrink_to_fit();
    sel_active = false;
    sel_start_line = 0;
    sel_start_col = 0;

    fold_regions.clear();
    fold_regions.shrink_to_fit();
    folded_lines.clear();

    if (highlighter.tree) {
        ts_tree_delete(highlighter.tree);
        highlighter.tree = nullptr;
    }

    highlighter.set_language_for_file(file_path, lines, line_offsets);

    return true;
}

void Editor::load_text(const std::string& text) {
    lines.clear();
    lines.shrink_to_fit();
    token_cache.clear();
    viewport_tokens_buffer.clear();
    line_offsets.clear();
    line_offsets.shrink_to_fit();

    const char* ptr = text.data();
    const char* end = ptr + text.size();
    const char* line_start = ptr;
    uint32_t current_offset = 0;

    while (ptr < end) {
        if (*ptr == '\n') {
            lines.emplace_back(line_start, ptr - line_start);
            line_offsets.push_back(current_offset);
            current_offset += static_cast<uint32_t>(ptr - line_start) + 1;
            line_start = ptr + 1;
        }
        ptr++;
    }

    if (line_start < end) {
        lines.emplace_back(line_start, end - line_start);
        line_offsets.push_back(current_offset);
        current_offset += static_cast<uint32_t>(end - line_start);
    } else if (lines.empty() || line_start == end) {
        lines.emplace_back("");
        line_offsets.push_back(current_offset);
    }

    line_offsets.push_back(current_offset);

    if (lines.empty()) {
        lines.push_back("");
    }

    cursor_line = 0;
    cursor_col = 0;
    scroll_y = 0;
    scroll_x = 0;
    modified = false;
    syntax_dirty = true;

    undo_stack.clear();
    undo_stack.shrink_to_fit();
    redo_stack.clear();
    redo_stack.shrink_to_fit();
    current_group_id = 0;
    in_undo_group = false;

    highlight_occurrences.clear();
    highlight_occurrences.shrink_to_fit();
    highlighted_identifier.clear();
    last_highlight_line = -1;
    last_highlight_col = -1;

    selection_stack.clear();
    selection_stack.shrink_to_fit();
    sel_active = false;
    sel_start_line = 0;
    sel_start_col = 0;

    fold_regions.clear();
    fold_regions.shrink_to_fit();
    folded_lines.clear();

    if (highlighter.tree) {
        ts_tree_delete(highlighter.tree);
        highlighter.tree = nullptr;
    }

    highlighter.set_language_for_file(file_path, lines, line_offsets);
}

bool Editor::is_identifier_node(TSNode node) const {
    if (ts_node_is_null(node)) return false;
    const char* type = ts_node_type(node);
    return strcmp(type, "identifier") == 0 ||
           strcmp(type, "field_identifier") == 0 ||
           strcmp(type, "type_identifier") == 0 ||
           strcmp(type, "destructor_name") == 0;
}

std::string Editor::get_node_text(TSNode node) const {
    if (ts_node_is_null(node)) return "";
    TSPoint start_point = ts_node_start_point(node);
    TSPoint end_point = ts_node_end_point(node);

    if (start_point.row == end_point.row) {
        if (start_point.row < lines.size()) {
            const std::string& line = lines[start_point.row];
            uint32_t start_col = std::min(start_point.column, static_cast<uint32_t>(line.size()));
            uint32_t end_col = std::min(end_point.column, static_cast<uint32_t>(line.size()));
            return line.substr(start_col, end_col - start_col);
        }
        return "";
    }

    std::string result;
    for (uint32_t row = start_point.row; row <= end_point.row && row < lines.size(); row++) {
        const std::string& line = lines[row];
        if (row == start_point.row) {
            result += line.substr(std::min(start_point.column, static_cast<uint32_t>(line.size())));
            result += '\n';
        } else if (row == end_point.row) {
            result += line.substr(0, std::min(end_point.column, static_cast<uint32_t>(line.size())));
        } else {
            result += line;
            result += '\n';
        }
    }
    return result;
}

TSNode Editor::get_identifier_at_cursor() {
    if (!highlighter.tree) return TSNode{};
    uint32_t byte_offset = get_byte_offset(cursor_line, cursor_col);
    TSNode root = ts_tree_root_node(highlighter.tree);
    TSNode node = ts_node_descendant_for_byte_range(root, byte_offset, byte_offset);
    if (is_identifier_node(node)) {
        return node;
    }
    if (cursor_col > 0) {
        uint32_t prev_byte = get_byte_offset(cursor_line, cursor_col - 1);
        node = ts_node_descendant_for_byte_range(root, prev_byte, prev_byte);
        if (is_identifier_node(node)) {
            return node;
        }
    }
    return TSNode{};
}

void Editor::collect_identifiers_recursive(TSNode node, const std::string& target_name, std::vector<HighlightRange>& results) {
    if (ts_node_is_null(node)) return;
    if (is_identifier_node(node)) {
        std::string name = get_node_text(node);
        if (name == target_name) {
            TSPoint start = ts_node_start_point(node);
            TSPoint end = ts_node_end_point(node);
            if (start.row == end.row) {
                results.push_back({static_cast<int>(start.row),
                                   static_cast<int>(start.column),
                                   static_cast<int>(end.column)});
            }
        }
    }
    uint32_t child_count = ts_node_child_count(node);
    for (uint32_t i = 0; i < child_count; i++) {
        collect_identifiers_recursive(ts_node_child(node, i), target_name, results);
    }
}

void Editor::update_highlight_occurrences() {
    if (cursor_line == last_highlight_line && cursor_col == last_highlight_col) {
        return;
    }
    last_highlight_line = cursor_line;
    last_highlight_col = cursor_col;
    highlight_occurrences.clear();
    highlighted_identifier.clear();

    if (lines.size() > MAX_LINES_FOR_HIGHLIGHT) {
        return;
    }

    if (!highlighter.tree) return;
    TSNode node = get_identifier_at_cursor();
    if (ts_node_is_null(node)) return;
    std::string name = get_node_text(node);
    if (name.empty()) return;
    highlighted_identifier = name;
    TSNode root = ts_tree_root_node(highlighter.tree);
    collect_identifiers_recursive(root, name, highlight_occurrences);
}

TSNode Editor::find_name_in_declarator(TSNode declarator, const std::string& target_name) const {
    if (ts_node_is_null(declarator)) return TSNode{};
    const char* type = ts_node_type(declarator);

    if (is_identifier_node(declarator)) {
        if (get_node_text(declarator) == target_name) return declarator;
        return TSNode{};
    }

    if (strcmp(type, "pointer_declarator") == 0 ||
        strcmp(type, "reference_declarator") == 0 ||
        strcmp(type, "array_declarator") == 0 ||
        strcmp(type, "init_declarator") == 0 ||
        strcmp(type, "parenthesized_declarator") == 0) {
        TSNode child = ts_node_child_by_field_name(declarator, "declarator", 10);
        if (!ts_node_is_null(child)) {
            return find_name_in_declarator(child, target_name);
        }
        uint32_t count = ts_node_child_count(declarator);
        for (uint32_t i = 0; i < count; i++) {
            TSNode res = find_name_in_declarator(ts_node_child(declarator, i), target_name);
            if (!ts_node_is_null(res)) return res;
        }
    }

    if (strcmp(type, "function_declarator") == 0) {
        TSNode child = ts_node_child_by_field_name(declarator, "declarator", 10);
        return find_name_in_declarator(child, target_name);
    }

    if (strcmp(type, "qualified_identifier") == 0) {
        TSNode name_node = ts_node_child_by_field_name(declarator, "name", 4);
        if (!ts_node_is_null(name_node) && get_node_text(name_node) == target_name) {
            return name_node;
        }
    }

    return TSNode{};
}

TSNode Editor::get_definition_name_node(TSNode node, const std::string& name) {
    if (ts_node_is_null(node)) return TSNode{};
    const char* type = ts_node_type(node);

    if (strcmp(type, "class_specifier") == 0 ||
        strcmp(type, "struct_specifier") == 0 ||
        strcmp(type, "enum_specifier") == 0 ||
        strcmp(type, "namespace_definition") == 0 ||
        strcmp(type, "class_definition") == 0 ||
        strcmp(type, "class_declaration") == 0) {
        TSNode name_node = ts_node_child_by_field_name(node, "name", 4);
        if (!ts_node_is_null(name_node) && get_node_text(name_node) == name) {
            return name_node;
        }
        return TSNode{};
    }

    if (strcmp(type, "declaration") == 0 ||
        strcmp(type, "field_declaration") == 0 ||
        strcmp(type, "parameter_declaration") == 0 ||
        strcmp(type, "variable_declaration") == 0 ||
        strcmp(type, "lexical_declaration") == 0) {
        uint32_t count = ts_node_child_count(node);
        for (uint32_t i = 0; i < count; i++) {
            TSNode child = ts_node_child(node, i);
            TSNode found = find_name_in_declarator(child, name);
            if (!ts_node_is_null(found)) return found;
        }
    }

    if (strcmp(type, "function_definition") == 0 ||
        strcmp(type, "function_declaration") == 0 ||
        strcmp(type, "method_definition") == 0) {
        TSNode name_node = ts_node_child_by_field_name(node, "name", 4);
        if (!ts_node_is_null(name_node) && get_node_text(name_node) == name) {
            return name_node;
        }
        TSNode declarator = ts_node_child_by_field_name(node, "declarator", 10);
        return find_name_in_declarator(declarator, name);
    }

    if (strcmp(type, "alias_declaration") == 0 || strcmp(type, "type_definition") == 0) {
        uint32_t count = ts_node_child_count(node);
        for (uint32_t i = 0; i < count; i++) {
            TSNode child = ts_node_child(node, i);
            if (strcmp(ts_node_type(child), "type_identifier") == 0 && get_node_text(child) == name) {
                return child;
            }
        }
    }

    if (strcmp(type, "template_parameter_list") == 0) {
        uint32_t count = ts_node_child_count(node);
        for (uint32_t i = 0; i < count; i++) {
            TSNode param = ts_node_child(node, i);
            TSNode found = find_name_in_declarator(param, name);
            if (!ts_node_is_null(found)) return found;
            if (strcmp(ts_node_type(param), "type_parameter_declaration") == 0) {
                TSNode name_node = ts_node_child_by_field_name(param, "name", 4);
                if (!ts_node_is_null(name_node) && get_node_text(name_node) == name) return name_node;
            }
        }
    }

    if (strcmp(type, "assignment") == 0 ||
        strcmp(type, "assignment_statement") == 0) {
        TSNode left = ts_node_child_by_field_name(node, "left", 4);
        if (!ts_node_is_null(left) && get_node_text(left) == name) {
            return left;
        }
    }

    if (strcmp(type, "variable_declarator") == 0 ||
        strcmp(type, "local_variable_declaration") == 0) {
        TSNode name_node = ts_node_child_by_field_name(node, "name", 4);
        if (!ts_node_is_null(name_node) && get_node_text(name_node) == name) {
            return name_node;
        }
    }

    if (strcmp(type, "local_function") == 0 ||
        strcmp(type, "function_statement") == 0) {
        TSNode name_node = ts_node_child_by_field_name(node, "name", 4);
        if (!ts_node_is_null(name_node) && get_node_text(name_node) == name) {
            return name_node;
        }
    }

    if (strcmp(type, "pair") == 0) {
        TSNode key = ts_node_child_by_field_name(node, "key", 3);
        if (!ts_node_is_null(key) && get_node_text(key) == name) {
            return key;
        }
    }

    return TSNode{};
}

TSNode Editor::find_definition_global(TSNode node, const std::string& name) {
    if (ts_node_is_null(node)) return TSNode{};

    TSNode def_node = get_definition_name_node(node, name);
    if (!ts_node_is_null(def_node)) return def_node;

    uint32_t count = ts_node_child_count(node);
    for (uint32_t i = 0; i < count; i++) {
        TSNode found = find_definition_global(ts_node_child(node, i), name);
        if (!ts_node_is_null(found)) return found;
    }
    return TSNode{};
}

bool Editor::go_to_definition() {
    if (!highlighter.tree) return false;

    TSNode cursor_node = get_identifier_at_cursor();
    if (ts_node_is_null(cursor_node)) return false;

    std::string name = get_node_text(cursor_node);
    if (name.empty()) return false;

    TSNode root = ts_tree_root_node(highlighter.tree);
    TSNode target_node = TSNode{};

    TSNode current_scope = ts_node_parent(cursor_node);
    while (!ts_node_is_null(current_scope)) {
        uint32_t child_count = ts_node_child_count(current_scope);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(current_scope, i);
            TSNode def = get_definition_name_node(child, name);
            if (!ts_node_is_null(def)) {
                if (ts_node_start_byte(def) != ts_node_start_byte(cursor_node)) {
                    target_node = def;
                    goto found;
                }
            }
        }
        current_scope = ts_node_parent(current_scope);
    }

    if (ts_node_is_null(target_node)) {
        target_node = find_definition_global(root, name);
        if (!ts_node_is_null(target_node) &&
            ts_node_start_byte(target_node) == ts_node_start_byte(cursor_node)) {
            target_node = TSNode{};
        }
    }

found:
    if (!ts_node_is_null(target_node)) {
        TSPoint start = ts_node_start_point(target_node);
        cursor_line = static_cast<int>(start.row);
        cursor_col = static_cast<int>(start.column);
        ensure_visible(10);
        clear_selection();
        return true;
    }

    return false;
}

void Editor::set_selection_from_node(TSNode node) {
    TSPoint start = ts_node_start_point(node);
    TSPoint end = ts_node_end_point(node);
    sel_start_line = static_cast<int>(start.row);
    sel_start_col = static_cast<int>(start.column);
    cursor_line = static_cast<int>(end.row);
    cursor_col = static_cast<int>(end.column);
    sel_active = true;
}

bool Editor::expand_selection() {
    if (!highlighter.tree) return false;

    uint32_t current_start_byte, current_end_byte;

    if (has_selection()) {
        int sel_start_l, sel_start_c, sel_end_l, sel_end_c;
        get_selection_bounds(sel_start_l, sel_start_c, sel_end_l, sel_end_c);
        current_start_byte = get_byte_offset(sel_start_l, sel_start_c);
        current_end_byte = get_byte_offset(sel_end_l, sel_end_c);
    } else {
        current_start_byte = get_byte_offset(cursor_line, cursor_col);
        current_end_byte = current_start_byte;
        if (selection_stack.empty()) {
            selection_stack.push_back({cursor_line, cursor_col, cursor_line, cursor_col});
        }
    }

    TSNode root = ts_tree_root_node(highlighter.tree);
    TSNode node = ts_node_descendant_for_byte_range(root, current_start_byte, current_end_byte);

    if (ts_node_is_null(node)) return false;

    while (!ts_node_is_null(node)) {
        uint32_t node_start = ts_node_start_byte(node);
        uint32_t node_end = ts_node_end_byte(node);

        if (node_start < current_start_byte || node_end > current_end_byte) {
            break;
        }

        TSNode parent = ts_node_parent(node);
        if (ts_node_is_null(parent)) return false;
        node = parent;
    }

    TSPoint start = ts_node_start_point(node);
    TSPoint end = ts_node_end_point(node);

    SelectionNode new_sel = {
        static_cast<int>(start.row), static_cast<int>(start.column),
        static_cast<int>(end.row), static_cast<int>(end.column)
    };

    selection_stack.push_back(new_sel);
    set_selection_from_node(node);
    return true;
}

bool Editor::shrink_selection() {
    if (selection_stack.size() <= 1) {
        clear_selection();
        selection_stack.clear();
        return false;
    }
    selection_stack.pop_back();
    const SelectionNode& prev = selection_stack.back();
    if (prev.start_line == prev.end_line && prev.start_col == prev.end_col) {
        clear_selection();
        cursor_line = prev.start_line;
        cursor_col = prev.start_col;
    } else {
        sel_start_line = prev.start_line;
        sel_start_col = prev.start_col;
        cursor_line = prev.end_line;
        cursor_col = prev.end_col;
        sel_active = true;
    }
    return true;
}

void Editor::reset_selection_stack() {
    selection_stack.clear();
}

bool Editor::is_foldable_node(TSNode node) const {
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

void Editor::collect_fold_regions_recursive(TSNode node) {
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
        collect_fold_regions_recursive(ts_node_child(node, i));
    }
}

void Editor::update_fold_regions() {
    if (!highlighter.tree) return;
    std::unordered_set<int> old_folded;
    for (const auto& fr : fold_regions) {
        if (fr.folded) {
            old_folded.insert(fr.start_line);
        }
    }
    fold_regions.clear();
    TSNode root = ts_tree_root_node(highlighter.tree);
    collect_fold_regions_recursive(root);
    for (auto& fr : fold_regions) {
        if (old_folded.count(fr.start_line)) {
            fr.folded = true;
        }
    }
    update_folded_lines();
}

void Editor::update_folded_lines() {
    folded_lines.clear();
    for (const auto& fr : fold_regions) {
        if (fr.folded) {
            for (int line = fr.start_line + 1; line <= fr.end_line; line++) {
                folded_lines.insert(line);
            }
        }
    }
}

FoldRegion* Editor::get_fold_region_at_line(int line) {
    for (auto& fr : fold_regions) {
        if (fr.start_line == line) {
            return &fr;
        }
    }
    return nullptr;
}

bool Editor::toggle_fold_at_line(int line) {
    FoldRegion* fr = get_fold_region_at_line(line);
    if (!fr) return false;
    fr->folded = !fr->folded;
    update_folded_lines();
    return true;
}

bool Editor::toggle_fold_at_cursor() {
    int line = cursor_line;
    if (toggle_fold_at_line(line)) {
        ensure_cursor_not_in_fold();
        return true;
    }
    for (const auto& fr : fold_regions) {
        if (cursor_line > fr.start_line && cursor_line <= fr.end_line) {
            if (toggle_fold_at_line(fr.start_line)) {
                ensure_cursor_not_in_fold();
                return true;
            }
        }
    }
    return false;
}

void Editor::fold_all() {
    for (auto& fr : fold_regions) {
        fr.folded = true;
    }
    update_folded_lines();
    ensure_cursor_not_in_fold();
}

void Editor::unfold_all() {
    for (auto& fr : fold_regions) {
        fr.folded = false;
    }
    update_folded_lines();
}

bool Editor::is_line_folded(int line) const {
    return folded_lines.count(line) > 0;
}

bool Editor::is_fold_start(int line) const {
    for (const auto& fr : fold_regions) {
        if (fr.start_line == line) {
            return true;
        }
    }
    return false;
}

bool Editor::is_fold_start_folded(int line) const {
    for (const auto& fr : fold_regions) {
        if (fr.start_line == line) {
            return fr.folded;
        }
    }
    return false;
}

int Editor::get_fold_end_line(int start_line) const {
    for (const auto& fr : fold_regions) {
        if (fr.start_line == start_line) {
            return fr.end_line;
        }
    }
    return start_line;
}

void Editor::ensure_cursor_not_in_fold() {
    if (!is_line_folded(cursor_line)) return;
    for (const auto& fr : fold_regions) {
        if (fr.folded && cursor_line > fr.start_line && cursor_line <= fr.end_line) {
            cursor_line = fr.start_line;
            cursor_col = std::min(cursor_col, static_cast<int>(lines[cursor_line].size()));
            return;
        }
    }
}

int Editor::get_next_visible_line(int from_line, int direction) const {
    int line = from_line + direction;
    while (line >= 0 && line < static_cast<int>(lines.size())) {
        if (!is_line_folded(line)) {
            return line;
        }
        line += direction;
    }
    return from_line;
}

void Editor::render(SDL_Renderer* renderer, TTF_Font* font, TextureCache& texture_cache,
                    const std::string& search_query,
                    int x_offset, int y_offset, int visible_width, int visible_height,
                    int window_w, int char_width,
                    bool has_focus, bool is_file_open, bool cursor_visible,
                    std::function<SDL_Color(TokenType)> syntax_color_func) {

    int visible_lines = (visible_height - PADDING * 2) / line_height;
    int text_x = x_offset + GUTTER_WIDTH + PADDING - scroll_x;
    int y = y_offset;

    SDL_SetRenderDrawColor(renderer, Colors::GUTTER.r, Colors::GUTTER.g, Colors::GUTTER.b, 255);
    SDL_Rect gutter_rect = {x_offset, y_offset, GUTTER_WIDTH, visible_height};
    SDL_RenderFillRect(renderer, &gutter_rect);

    char line_num_buf[16];
    for (int i = scroll_y; i < static_cast<int>(lines.size()) && y < visible_height; i++) {
        if (is_line_folded(i)) continue;

        bool is_cursor_line = (i == cursor_line) && is_file_open;
        if (is_cursor_line && has_focus) {
            SDL_SetRenderDrawColor(renderer, Colors::ACTIVE_LINE.r, Colors::ACTIVE_LINE.g, Colors::ACTIVE_LINE.b, 255);
            SDL_Rect active_gutter_rect = {x_offset, y, GUTTER_WIDTH, line_height};
            SDL_RenderFillRect(renderer, &active_gutter_rect);
        }

        snprintf(line_num_buf, sizeof(line_num_buf), "%d", i + 1);
        SDL_Color num_color = is_cursor_line ? Colors::LINE_NUM_ACTIVE : Colors::LINE_NUM;
        texture_cache.render_cached_text_right_aligned(line_num_buf, num_color, x_offset + GUTTER_WIDTH - 8, y);

        if (is_fold_start(i)) {
            const char* marker = is_fold_start_folded(i) ? "▶" : "▼";
            texture_cache.render_cached_text(marker, Colors::FOLD_INDICATOR, x_offset + 4, y);
        }
        y += line_height;
    }

    SDL_Rect text_clip = {x_offset + GUTTER_WIDTH, y_offset, visible_width, visible_height};
    SDL_RenderSetClipRect(renderer, &text_clip);

    if (syntax_dirty) {
        bool is_large_file = lines.size() > LARGE_FILE_LINES;
        if (!is_large_file || (SDL_GetTicks() - last_edit_time > SYNTAX_DEBOUNCE_MS)) {
            rebuild_syntax();
        }
    }
    prefetch_viewport_tokens(scroll_y, visible_lines + 5);

    y = y_offset;
    for (int i = scroll_y; i < static_cast<int>(lines.size()) && y < visible_height; i++) {
        if (is_line_folded(i)) continue;

        if (i == cursor_line && is_file_open && has_focus) {
            SDL_SetRenderDrawColor(renderer, Colors::ACTIVE_LINE.r, Colors::ACTIVE_LINE.g, Colors::ACTIVE_LINE.b, 255);
            SDL_Rect active_line_rect = {x_offset + GUTTER_WIDTH, y, visible_width, line_height};
            SDL_RenderFillRect(renderer, &active_line_rect);
        }

        for (const auto& hl : highlight_occurrences) {
            if (hl.line == i) {
                int hl_x_start = text_x;
                if (hl.start_col > 0) {
                    int w = 0;
                    TTF_SizeUTF8(font, lines[i].substr(0, hl.start_col).c_str(), &w, nullptr);
                    hl_x_start += w;
                }
                int hl_w = 0;
                TTF_SizeUTF8(font, lines[i].substr(hl.start_col, hl.end_col - hl.start_col).c_str(), &hl_w, nullptr);
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(renderer, Colors::OCCURRENCE_HIGHLIGHT.r, Colors::OCCURRENCE_HIGHLIGHT.g, Colors::OCCURRENCE_HIGHLIGHT.b, Colors::OCCURRENCE_HIGHLIGHT.a);
                SDL_Rect hl_rect = {hl_x_start, y, hl_w, line_height};
                SDL_RenderFillRect(renderer, &hl_rect);
            }
        }

        if (has_selection()) {
            int sel_start_line_v, sel_start_col_v, sel_end_line_v, sel_end_col_v;
            get_selection_bounds(sel_start_line_v, sel_start_col_v, sel_end_line_v, sel_end_col_v);
            if (i >= sel_start_line_v && i <= sel_end_line_v) {
                int line_start = (i == sel_start_line_v) ? sel_start_col_v : 0;
                int line_end = (i == sel_end_line_v) ? sel_end_col_v : static_cast<int>(lines[i].size());
                int x_start = text_x;
                if (line_start > 0) {
                    int w = 0;
                    TTF_SizeUTF8(font, lines[i].substr(0, line_start).c_str(), &w, nullptr);
                    x_start += w;
                }
                int sel_w = 0;
                if (line_end > line_start) {
                    TTF_SizeUTF8(font, lines[i].substr(line_start, line_end - line_start).c_str(), &sel_w, nullptr);
                }
                if (i < sel_end_line_v && line_end == static_cast<int>(lines[i].size())) {
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

        if (!search_query.empty() && !lines[i].empty()) {
            size_t pos = 0;
            while ((pos = lines[i].find(search_query, pos)) != std::string::npos) {
                int x_start = text_x;
                if (pos > 0) {
                    int w = 0;
                    TTF_SizeUTF8(font, lines[i].substr(0, pos).c_str(), &w, nullptr);
                    x_start += w;
                }
                int highlight_w = 0;
                TTF_SizeUTF8(font, search_query.c_str(), &highlight_w, nullptr);
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(renderer, Colors::SEARCH_HIGHLIGHT.r, Colors::SEARCH_HIGHLIGHT.g, Colors::SEARCH_HIGHLIGHT.b, Colors::SEARCH_HIGHLIGHT.a);
                SDL_Rect highlight_rect = {x_start, y, highlight_w, line_height};
                SDL_RenderFillRect(renderer, &highlight_rect);
                pos += search_query.size();
            }
        }

        if (!lines[i].empty()) {
            const std::string& line_text = lines[i];
            const std::vector<Token>& tokens = get_line_tokens(i);

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

                SDL_Surface* surf = texture_cache.render_line_to_surface(sub_text, sub_tokens, Colors::TEXT, syntax_color_func);
                if (surf) {
                    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
                    if (tex) {
                        int offset_x_local = start_char_idx * char_width;
                        SDL_Rect dst = {text_x + offset_x_local, y, surf->w, surf->h};
                        SDL_RenderCopy(renderer, tex, nullptr, &dst);
                        SDL_DestroyTexture(tex);
                    }
                    SDL_FreeSurface(surf);
                }
            } else {
                CachedLineRender& cached = texture_cache.get_or_build_line_render(
                    i, line_text, tokens, Colors::TEXT, syntax_color_func
                );
                texture_cache.render_cached_line(cached, text_x, y);
            }
        }

        if (is_fold_start_folded(i)) {
            int fold_end = get_fold_end_line(i);
            char fold_buf[32];
            snprintf(fold_buf, sizeof(fold_buf), " ... (%d lines)", fold_end - i);
            int line_w = 0;
            if (!lines[i].empty()) {
                TTF_SizeUTF8(font, lines[i].c_str(), &line_w, nullptr);
            }
            texture_cache.render_cached_text(fold_buf, Colors::FOLD_INDICATOR, text_x + line_w, y);
        }

        if (i == cursor_line && cursor_visible && is_file_open && has_focus) {
            int cursor_x_local = text_x;
            if (cursor_col > 0 && !lines[i].empty()) {
                std::string before_cursor = lines[i].substr(0, cursor_col);
                int w = 0;
                TTF_SizeUTF8(font, before_cursor.c_str(), &w, nullptr);
                cursor_x_local += w;
            }
            SDL_SetRenderDrawColor(renderer, Colors::CURSOR.r, Colors::CURSOR.g, Colors::CURSOR.b, 255);
            SDL_Rect cursor_rect = {cursor_x_local, y, 2, line_height};
            SDL_RenderFillRect(renderer, &cursor_rect);
        }

        y += line_height;
    }

    SDL_RenderSetClipRect(renderer, nullptr);
}

void Editor::handle_scroll(int wheel_x, int wheel_y, int char_w, bool shift_held) {
    if (shift_held) {
        scroll_x += wheel_y * char_w * 3;
        scroll_x = std::max(0, scroll_x);
    } else if (wheel_x != 0) {
        scroll_x -= wheel_x * char_w * 3;
        scroll_x = std::max(0, scroll_x);
    } else {
        int scroll_amount = -wheel_y * 3;
        scroll_y = get_nth_visible_line_from(scroll_y, scroll_amount);
        scroll_y = std::max(0, scroll_y);
        int max_scroll = std::max(0, static_cast<int>(lines.size()) - 1);
        scroll_y = std::min(scroll_y, max_scroll);
        scroll_y = get_first_visible_line_from(scroll_y);
    }
}

void Editor::update_cursor_from_mouse(int x, int y, int x_offset, int y_offset, TTF_Font* font) {
    int text_area_x = x_offset + GUTTER_WIDTH;

    if (x < text_area_x) x = text_area_x;

    int relative_y = y - y_offset;
    if (relative_y < 0) relative_y = 0;

    int visual_line_index = relative_y / line_height;
    int target_line = scroll_y;
    int visual_count = 0;

    while (target_line < static_cast<int>(lines.size()) && visual_count < visual_line_index) {
        if (!is_line_folded(target_line)) {
            visual_count++;
        }
        target_line++;
    }

    while (target_line < static_cast<int>(lines.size()) && is_line_folded(target_line)) {
        target_line++;
    }

    if (target_line >= static_cast<int>(lines.size())) {
        target_line = static_cast<int>(lines.size()) - 1;
    }

    cursor_line = target_line;

    int text_x = x_offset + GUTTER_WIDTH + PADDING - scroll_x;
    int click_x = x - text_x;

    if (click_x <= 0 || lines[cursor_line].empty()) {
        cursor_col = 0;
    } else {
        const std::string& line = lines[cursor_line];
        int best_col = 0;
        int best_diff = click_x;

        for (size_t col = 1; col <= line.size(); col++) {
            int w = 0;
            TTF_SizeUTF8(font, line.substr(0, col).c_str(), &w, nullptr);
            int diff = std::abs(click_x - w);
            if (diff < best_diff) {
                best_diff = diff;
                best_col = static_cast<int>(col);
            }
        }
        cursor_col = best_col;
    }
}

void Editor::handle_mouse_click(int x, int y, int x_offset, int y_offset, TTF_Font* font) {
    clear_selection();
    update_cursor_from_mouse(x, y, x_offset, y_offset, font);
    start_selection();
}

void Editor::handle_mouse_double_click(int x, int y, int x_offset, int y_offset, TTF_Font* font) {
    update_cursor_from_mouse(x, y, x_offset, y_offset, font);
    select_word_at_cursor();
}

void Editor::handle_mouse_drag(int x, int y, int x_offset, int y_offset, TTF_Font* font) {
    update_cursor_from_mouse(x, y, x_offset, y_offset, font);
}

void Editor::select_word_at_cursor() {
    const std::string& line = lines[cursor_line];
    if (line.empty()) return;

    int line_len = static_cast<int>(line.size());
    if (cursor_col >= line_len) cursor_col = line_len > 0 ? line_len - 1 : 0;

    if (!is_word_char_at(line, cursor_col)) {
        sel_start_line = cursor_line;
        sel_start_col = cursor_col;
        cursor_col = utf8_next_char_pos(line, cursor_col);
        sel_active = true;
        return;
    }

    int word_start = cursor_col;
    while (word_start > 0) {
        int prev = utf8_prev_char_start(line, word_start);
        if (!is_word_char_at(line, prev)) break;
        word_start = prev;
    }

    int word_end = cursor_col;
    while (word_end < line_len && is_word_char_at(line, word_end)) {
        word_end = utf8_next_char_pos(line, word_end);
    }

    sel_start_line = cursor_line;
    sel_start_col = word_start;
    cursor_col = word_end;
    sel_active = true;
}

Editor::KeyResult Editor::handle_key(const SDL_Event& event, int visible_lines) {
    KeyResult result;
    if (readonly) return result;

    bool ctrl = (event.key.keysym.mod & META_MOD) != 0;
    bool shift = (event.key.keysym.mod & KMOD_SHIFT) != 0;
    bool alt = (event.key.keysym.mod & KMOD_ALT) != 0;

    switch (event.key.keysym.sym) {
        case SDLK_RETURN:
            clear_selection();
            reset_selection_stack();
            new_line();
            result.consumed = result.cursor_moved = true;
            break;
        case SDLK_BACKSPACE:
            reset_selection_stack();
            if (ctrl) delete_word_left();
            else backspace();
            result.consumed = result.cursor_moved = true;
            break;
        case SDLK_DELETE:
            reset_selection_stack();
            if (ctrl) delete_word_right();
            else delete_char();
            result.consumed = result.cursor_moved = true;
            break;
        case SDLK_LEFT:
            if (shift) start_selection();
            else clear_selection();
            reset_selection_stack();
            if (ctrl) move_word_left();
            else move_left();
            result.consumed = result.cursor_moved = true;
            break;
        case SDLK_RIGHT:
            if (shift) start_selection();
            else clear_selection();
            reset_selection_stack();
            if (ctrl) move_word_right();
            else move_right();
            result.consumed = result.cursor_moved = true;
            break;
        case SDLK_UP:
            if (alt) {
                move_line_up();
            } else {
                if (shift) start_selection();
                else clear_selection();
                reset_selection_stack();
                move_up();
            }
            result.consumed = result.cursor_moved = true;
            break;
        case SDLK_DOWN:
            if (alt) {
                move_line_down();
            } else {
                if (shift) start_selection();
                else clear_selection();
                reset_selection_stack();
                move_down();
            }
            result.consumed = result.cursor_moved = true;
            break;
        case SDLK_HOME:
            if (shift) start_selection();
            else clear_selection();
            reset_selection_stack();
            move_home();
            result.consumed = result.cursor_moved = true;
            break;
        case SDLK_END:
            if (shift) start_selection();
            else clear_selection();
            reset_selection_stack();
            move_end();
            result.consumed = result.cursor_moved = true;
            break;
        case SDLK_PAGEUP:
            if (shift) start_selection();
            else clear_selection();
            move_page_up(visible_lines);
            result.consumed = result.cursor_moved = true;
            break;
        case SDLK_PAGEDOWN:
            if (shift) start_selection();
            else clear_selection();
            move_page_down(visible_lines);
            result.consumed = result.cursor_moved = true;
            break;
        case SDLK_a:
            if (ctrl) {
                sel_start_line = 0;
                sel_start_col = 0;
                cursor_line = static_cast<int>(lines.size()) - 1;
                cursor_col = static_cast<int>(lines.back().size());
                sel_active = true;
                result.consumed = result.cursor_moved = true;
            }
            break;
        case SDLK_c:
            if (ctrl && has_selection()) {
                SDL_SetClipboardText(get_selected_text().c_str());
                result.consumed = true;
            }
            break;
        case SDLK_x:
            if (ctrl && has_selection()) {
                SDL_SetClipboardText(get_selected_text().c_str());
                delete_selection();
                result.consumed = result.cursor_moved = true;
            }
            break;
        case SDLK_v:
            if (ctrl && SDL_HasClipboardText()) {
                char* clipboard = SDL_GetClipboardText();
                begin_undo_group();
                insert_text(clipboard);
                end_undo_group();
                SDL_free(clipboard);
                result.consumed = result.cursor_moved = true;
            }
            break;
        case SDLK_d:
            if (ctrl) {
                duplicate_line();
                result.consumed = result.cursor_moved = true;
            }
            break;
        case SDLK_z:
            if (ctrl && shift) {
                if (redo()) result.cursor_moved = true;
                result.consumed = true;
            } else if (ctrl) {
                if (undo()) result.cursor_moved = true;
                result.consumed = true;
            }
            break;
        case SDLK_y:
            if (ctrl) {
                if (redo()) result.cursor_moved = true;
                result.consumed = true;
            }
            break;
        case SDLK_SLASH:
            if (ctrl) {
                toggle_comment();
                result.consumed = result.cursor_moved = true;
            }
            break;
        case SDLK_TAB:
            if (!ctrl) {
                insert_text("    ");
                result.consumed = result.cursor_moved = true;
            }
            break;
        case SDLK_F12:
            if (go_to_definition()) {
                result.cursor_moved = true;
            }
            result.consumed = true;
            break;
        case SDLK_w:
            if (ctrl && shift) {
                shrink_selection();
                result.consumed = result.cursor_moved = true;
            } else if (ctrl) {
                expand_selection();
                result.consumed = result.cursor_moved = true;
            }
            break;
        case SDLK_LEFTBRACKET:
            if (ctrl && shift) {
                toggle_fold_at_cursor();
                result.consumed = true;
            }
            break;
        case SDLK_RIGHTBRACKET:
            if (ctrl && shift) {
                unfold_all();
                result.consumed = true;
            }
            break;
        case SDLK_k:
            if (ctrl && shift) {
                fold_all();
                result.consumed = true;
            }
            break;
    }

    return result;
}
