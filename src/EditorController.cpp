#include "EditorController.h"
#include "Utils.h"
#include <algorithm>
#include <climits>
#include <cstring>

bool EditorController::has_selection() const {
    return sel_active && (sel_start_line != cursor_line || sel_start_col != cursor_col);
}

void EditorController::clear_selection() {
    sel_active = false;
}

void EditorController::start_selection() {
    if (!sel_active) {
        sel_start_line = cursor_line;
        sel_start_col = cursor_col;
        sel_active = true;
    }
}

TextRange EditorController::get_selection_range() const {
    if (cursor_line < sel_start_line || (cursor_line == sel_start_line && cursor_col < sel_start_col)) {
        return {{cursor_line, cursor_col}, {sel_start_line, sel_start_col}};
    }
    return {{sel_start_line, sel_start_col}, {cursor_line, cursor_col}};
}

std::string EditorController::get_selected_text(const TextDocument& doc) const {
    if (!has_selection()) return "";
    auto sel = get_selection_range();
    std::string result;
    for (int i = sel.start.line; i <= sel.end.line; i++) {
        int col_start = (i == sel.start.line) ? sel.start.col : 0;
        int col_end = (i == sel.end.line) ? sel.end.col : static_cast<int>(doc.lines[i].size());
        result += doc.lines[i].substr(col_start, col_end - col_start);
        if (i < sel.end.line) result += '\n';
    }
    return result;
}

void EditorController::begin_undo_group() {
    if (!in_undo_group) {
        current_group_id++;
        in_undo_group = true;
    }
}

void EditorController::end_undo_group() {
    in_undo_group = false;
}

uint64_t EditorController::get_undo_group_id() {
    if (!in_undo_group) {
        current_group_id++;
    }
    return current_group_id;
}

void EditorController::push_action(EditAction action) {
    command_manager.push(std::move(action));
}

bool EditorController::undo(TextDocument& doc, EditorView& view) {
    if (!command_manager.can_undo()) return false;

    auto& undo_stack = command_manager.get_undo_stack();
    auto& redo_stack = command_manager.get_redo_stack();

    uint64_t group = get_action_group_id(undo_stack.back());

    while (!undo_stack.empty() && get_action_group_id(undo_stack.back()) == group) {
        EditAction action = std::move(undo_stack.back());
        undo_stack.pop_back();

        std::visit([this, &doc](auto& op) {
            revert_action(op, doc, *this);
        }, action);

        redo_stack.push_back(std::move(action));
    }

    clear_selection();
    view.mark_syntax_dirty();
    return true;
}

bool EditorController::redo(TextDocument& doc, EditorView& view) {
    if (!command_manager.can_redo()) return false;

    auto& undo_stack = command_manager.get_undo_stack();
    auto& redo_stack = command_manager.get_redo_stack();

    uint64_t group = get_action_group_id(redo_stack.back());

    while (!redo_stack.empty() && get_action_group_id(redo_stack.back()) == group) {
        EditAction action = std::move(redo_stack.back());
        redo_stack.pop_back();

        std::visit([this, &doc](auto& op) {
            apply_action(op, doc, *this);
        }, action);

        undo_stack.push_back(std::move(action));
    }

    clear_selection();
    view.mark_syntax_dirty();
    return true;
}

void EditorController::delete_selection(TextDocument& doc, EditorView& view) {
    if (doc.readonly) return;
    if (!has_selection()) return;

    auto sel = get_selection_range();
    std::string deleted_text = get_selected_text(doc);
    uint64_t group = get_undo_group_id();

    std::string out_deleted;
    doc.delete_range(sel.start, sel.end, out_deleted);

    set_cursor_pos(sel.start);
    clear_selection();
    view.mark_syntax_dirty();

    push_action(DeleteOp{sel.start.line, sel.start.col, deleted_text, sel.end.line, sel.end.col, group});
}

void EditorController::insert_text(TextDocument& doc, EditorView& view, const char* text) {
    if (doc.readonly) return;
    if (has_selection()) {
        delete_selection(doc, view);
    }

    std::string str(text);
    if (str.empty()) return;

    const auto& auto_pairs = view.highlighter.get_auto_pairs();

    if (str.size() == 1) {
        char ch = str[0];
        const std::string& current_line = doc.lines[cursor_line];

        if (is_closing_char(ch, auto_pairs)) {
            if (cursor_col < static_cast<int>(current_line.size()) && current_line[cursor_col] == ch) {
                cursor_col++;
                return;
            }
        }

        char closing = get_closing_pair(ch, auto_pairs);
        if (closing != '\0') {
            str += closing;
        }
    }

    int start_line_pos = cursor_line;
    int start_col_pos = cursor_col;
    uint64_t group = get_undo_group_id();

    int final_cursor_offset = 0;
    if (str.size() == 2 && get_closing_pair(str[0], auto_pairs) == str[1]) {
        final_cursor_offset = 1;
    }

    TextPos end_pos;
    doc.insert_at(cursor_pos(), str, end_pos);

    cursor_line = end_pos.line;
    cursor_col = end_pos.col - final_cursor_offset;

    view.mark_syntax_dirty();

    push_action(InsertOp{start_line_pos, start_col_pos, str, end_pos.line, end_pos.col, group});
}

void EditorController::new_line(TextDocument& doc, EditorView& view) {
    if (doc.readonly) return;

    std::string indent;
    const std::string& current_text = doc.lines[cursor_line];
    for (char c : current_text) {
        if (c == ' ' || c == '\t') {
            indent += c;
        } else {
            break;
        }
    }

    const auto& indent_triggers = view.highlighter.get_indent_triggers();
    if (cursor_col > 0 && !indent_triggers.empty()) {
        char prev_char = current_text[cursor_col - 1];
        for (char trigger : indent_triggers) {
            if (prev_char == trigger) {
                indent += "    ";
                break;
            }
        }
    }

    int start_line_pos = cursor_line;
    int start_col_pos = cursor_col;
    uint64_t group = get_undo_group_id();

    std::string insert_str = "\n" + indent;
    TextPos end_pos;
    doc.insert_at(cursor_pos(), insert_str, end_pos);

    cursor_line = end_pos.line;
    cursor_col = end_pos.col;

    view.mark_syntax_dirty();

    push_action(InsertOp{start_line_pos, start_col_pos, insert_str, cursor_line, cursor_col, group});
}

void EditorController::backspace(TextDocument& doc, EditorView& view) {
    if (doc.readonly) return;
    if (has_selection()) {
        delete_selection(doc, view);
        return;
    }

    if (cursor_col > 0) {
        int prev_pos = utf8_prev_char_start(doc.lines[cursor_line], cursor_col);
        const std::string& current_line = doc.lines[cursor_line];
        const auto& auto_pairs = view.highlighter.get_auto_pairs();

        bool delete_pair = false;
        if (cursor_col < static_cast<int>(current_line.size())) {
            char left_char = current_line[prev_pos];
            char right_char = current_line[cursor_col];
            if (get_closing_pair(left_char, auto_pairs) == right_char && right_char != '\0') {
                delete_pair = true;
            }
        }

        int delete_end = delete_pair ? cursor_col + 1 : cursor_col;
        std::string deleted_text = current_line.substr(prev_pos, delete_end - prev_pos);
        uint64_t group = get_undo_group_id();

        std::string out_deleted;
        doc.delete_range({cursor_line, prev_pos}, {cursor_line, delete_end}, out_deleted);
        cursor_col = prev_pos;

        view.mark_syntax_dirty();

        push_action(DeleteOp{cursor_line, prev_pos, deleted_text, cursor_line, delete_end, group});
    } else if (cursor_line > 0) {
        int orig_line = cursor_line;
        uint64_t group = get_undo_group_id();

        int new_col = static_cast<int>(doc.lines[cursor_line - 1].size());

        std::string out_deleted;
        doc.delete_range({cursor_line - 1, new_col}, {cursor_line, 0}, out_deleted);

        cursor_line--;
        cursor_col = new_col;

        view.mark_syntax_dirty();

        push_action(DeleteOp{cursor_line, new_col, "\n", orig_line, 0, group});
    }
}

void EditorController::delete_char(TextDocument& doc, EditorView& view) {
    if (doc.readonly) return;
    if (has_selection()) {
        delete_selection(doc, view);
        return;
    }

    if (cursor_col < static_cast<int>(doc.lines[cursor_line].size())) {
        int next_pos = utf8_next_char_pos(doc.lines[cursor_line], cursor_col);
        std::string deleted_text = doc.lines[cursor_line].substr(cursor_col, next_pos - cursor_col);
        uint64_t group = get_undo_group_id();

        std::string out_deleted;
        doc.delete_range({cursor_line, cursor_col}, {cursor_line, next_pos}, out_deleted);

        view.mark_syntax_dirty();

        push_action(DeleteOp{cursor_line, cursor_col, deleted_text, cursor_line, next_pos, group});
    } else if (cursor_line < static_cast<int>(doc.lines.size()) - 1) {
        uint64_t group = get_undo_group_id();

        std::string out_deleted;
        doc.delete_range({cursor_line, cursor_col}, {cursor_line + 1, 0}, out_deleted);

        view.mark_syntax_dirty();

        push_action(DeleteOp{cursor_line, cursor_col, "\n", cursor_line + 1, 0, group});
    }
}

void EditorController::toggle_comment(TextDocument& doc, EditorView& view) {
    if (doc.readonly) return;
    const std::string& comment_token = view.highlighter.get_line_comment_token();
    if (comment_token.empty()) return;

    int start_line = cursor_line;
    int end_line = cursor_line;

    if (has_selection()) {
        auto sel = get_selection_range();
        start_line = sel.start.line;
        end_line = sel.end.line;
    }

    bool all_commented = true;
    for (int i = start_line; i <= end_line; ++i) {
        const std::string& line = doc.lines[i];
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
            const std::string& line = doc.lines[i];
            size_t first_non_space = line.find_first_not_of(" \t");
            if (first_non_space == std::string::npos) continue;
            min_indent = std::min(min_indent, static_cast<int>(first_non_space));
        }
        if (min_indent == INT_MAX) min_indent = 0;
    }

    for (int i = start_line; i <= end_line; ++i) {
        std::string& line = doc.lines[i];
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
                std::string out_deleted;
                doc.delete_range({i, del_start}, {i, del_start + del_len}, out_deleted);

                push_action(DeleteOp{i, del_start, deleted, i, del_start + del_len, current_group_id});
            }
        } else {
            std::string insert_str = comment_token + " ";
            int insert_pos = min_indent;

            TextPos end_pos;
            doc.insert_at({i, insert_pos}, insert_str, end_pos);

            push_action(InsertOp{i, insert_pos, insert_str, i, insert_pos + static_cast<int>(insert_str.size()), current_group_id});
        }
    }

    end_undo_group();
    view.mark_syntax_dirty();
}

void EditorController::duplicate_line(TextDocument& doc, EditorView& view) {
    if (doc.readonly) return;

    if (has_selection()) {
        std::string selected = get_selected_text(doc);
        auto sel = get_selection_range();
        set_cursor_pos(sel.end);
        clear_selection();
        insert_text(doc, view, selected.c_str());
    } else {
        std::string current = doc.lines[cursor_line];
        int orig_line = cursor_line;
        uint64_t group = get_undo_group_id();

        std::string insert_str = "\n" + current;
        TextPos end_pos;
        doc.insert_at({cursor_line, static_cast<int>(current.size())}, insert_str, end_pos);

        cursor_line = end_pos.line;

        view.mark_syntax_dirty();

        push_action(InsertOp{orig_line, static_cast<int>(current.size()), insert_str, cursor_line, static_cast<int>(current.size()), group});
    }
}

void EditorController::move_left(const TextDocument& doc) {
    if (cursor_col > 0) {
        cursor_col = utf8_prev_char_start(doc.lines[cursor_line], cursor_col);
    }
}

void EditorController::move_right(const TextDocument& doc) {
    if (cursor_col < static_cast<int>(doc.lines[cursor_line].size())) {
        cursor_col = utf8_next_char_pos(doc.lines[cursor_line], cursor_col);
    }
}

bool EditorController::is_word_char_at(const std::string& str, ColIdx pos) const {
    return is_word_codepoint(utf8_decode_at(str, pos));
}

void EditorController::move_word_left(const TextDocument& doc) {
    const std::string& line = doc.lines[cursor_line];

    if (cursor_col == 0) {
        if (cursor_line > 0) {
            cursor_line--;
            cursor_col = static_cast<int>(doc.lines[cursor_line].size());
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

void EditorController::move_word_right(const TextDocument& doc) {
    const std::string& line = doc.lines[cursor_line];
    int line_len = static_cast<int>(line.size());

    if (cursor_col >= line_len) {
        if (cursor_line < static_cast<int>(doc.lines.size()) - 1) {
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

void EditorController::move_up(const TextDocument& doc, const EditorView& view) {
    if (cursor_line > 0) {
        int new_line = view.get_next_visible_line(cursor_line, -1, doc);
        if (new_line != cursor_line) {
            cursor_line = new_line;
            cursor_col = utf8_clamp_to_char_boundary(doc.lines[cursor_line], cursor_col);
        }
    }
}

void EditorController::move_down(const TextDocument& doc, const EditorView& view) {
    if (cursor_line < static_cast<int>(doc.lines.size()) - 1) {
        int new_line = view.get_next_visible_line(cursor_line, 1, doc);
        if (new_line != cursor_line) {
            cursor_line = new_line;
            cursor_col = utf8_clamp_to_char_boundary(doc.lines[cursor_line], cursor_col);
        }
    }
}

void EditorController::move_home() {
    cursor_col = 0;
}

void EditorController::move_end(const TextDocument& doc) {
    cursor_col = static_cast<int>(doc.lines[cursor_line].size());
}

void EditorController::move_page_up(const TextDocument& doc, int visible_lines) {
    cursor_line = std::max(0, cursor_line - visible_lines);
    cursor_col = utf8_clamp_to_char_boundary(doc.lines[cursor_line], cursor_col);
}

void EditorController::move_page_down(const TextDocument& doc, int visible_lines) {
    cursor_line = std::min(static_cast<int>(doc.lines.size()) - 1, cursor_line + visible_lines);
    cursor_col = utf8_clamp_to_char_boundary(doc.lines[cursor_line], cursor_col);
}

void EditorController::move_line_up(TextDocument& doc, EditorView& view) {
    if (doc.readonly) return;

    int block_start, block_end;

    if (has_selection()) {
        auto sel = get_selection_range();
        block_start = sel.start.line;
        block_end = sel.end.line;
    } else {
        block_start = cursor_line;
        block_end = cursor_line;
    }

    if (block_start <= 0) return;

    doc.move_lines(block_start, block_end, -1);

    cursor_line--;
    if (has_selection()) {
        sel_start_line--;
    }

    view.mark_syntax_dirty();

    push_action(MoveLineOp{block_start, block_end, -1, get_undo_group_id()});
}

void EditorController::move_line_down(TextDocument& doc, EditorView& view) {
    if (doc.readonly) return;

    int block_start, block_end;

    if (has_selection()) {
        auto sel = get_selection_range();
        block_start = sel.start.line;
        block_end = sel.end.line;
    } else {
        block_start = cursor_line;
        block_end = cursor_line;
    }

    if (block_end >= static_cast<int>(doc.lines.size()) - 1) return;

    doc.move_lines(block_start, block_end, 1);

    cursor_line++;
    if (has_selection()) {
        sel_start_line++;
    }

    view.mark_syntax_dirty();

    push_action(MoveLineOp{block_start, block_end, 1, get_undo_group_id()});
}

void EditorController::delete_word_left(TextDocument& doc, EditorView& view) {
    if (doc.readonly) return;
    if (has_selection()) {
        delete_selection(doc, view);
        return;
    }

    int orig_line = cursor_line;
    int orig_col = cursor_col;

    move_word_left(doc);

    if (cursor_line != orig_line || cursor_col != orig_col) {
        sel_start_line = orig_line;
        sel_start_col = orig_col;
        sel_active = true;
        delete_selection(doc, view);
    }
}

void EditorController::delete_word_right(TextDocument& doc, EditorView& view) {
    if (doc.readonly) return;
    if (has_selection()) {
        delete_selection(doc, view);
        return;
    }

    int orig_line = cursor_line;
    int orig_col = cursor_col;

    move_word_right(doc);

    if (cursor_line != orig_line || cursor_col != orig_col) {
        sel_start_line = orig_line;
        sel_start_col = orig_col;
        sel_active = true;
        delete_selection(doc, view);
    }
}

void EditorController::go_to(const TextDocument& doc, TextPos pos) {
    cursor_line = std::max(0, std::min(pos.line - 1, static_cast<int>(doc.lines.size()) - 1));
    cursor_col = utf8_clamp_to_char_boundary(doc.lines[cursor_line], pos.col > 0 ? pos.col - 1 : 0);
    clear_selection();
}

bool EditorController::find_next(const TextDocument& doc, const std::string& query, TextPos start) {
    if (query.empty()) return false;
    clear_selection();
    for (int i = start.line; i < static_cast<int>(doc.lines.size()); i++) {
        size_t search_start = (i == start.line) ? start.col : 0;
        size_t pos = doc.lines[i].find(query, search_start);
        if (pos != std::string::npos) {
            cursor_line = i;
            cursor_col = static_cast<int>(pos);
            return true;
        }
    }
    for (int i = 0; i <= start.line; i++) {
        size_t end_col = (i == start.line) ? start.col : doc.lines[i].size();
        size_t pos = doc.lines[i].find(query);
        if (pos != std::string::npos && pos < end_col) {
            cursor_line = i;
            cursor_col = static_cast<int>(pos);
            return true;
        }
    }
    return false;
}

char EditorController::get_closing_pair(char c, const std::vector<AutoPair>& pairs) {
    for (const auto& pair : pairs) {
        if (pair.open == c) return pair.close;
    }
    return '\0';
}

bool EditorController::is_closing_char(char c, const std::vector<AutoPair>& pairs) {
    for (const auto& pair : pairs) {
        if (pair.close == c) return true;
    }
    return false;
}

char EditorController::get_opening_pair(char c, const std::vector<AutoPair>& pairs) {
    for (const auto& pair : pairs) {
        if (pair.close == c) return pair.open;
    }
    return '\0';
}

void EditorController::select_word_at_cursor(const TextDocument& doc) {
    const std::string& line = doc.lines[cursor_line];
    if (line.empty()) return;

    int line_len = static_cast<int>(line.size());
    if (cursor_col >= line_len) cursor_col = utf8_prev_char_start(line, line_len);

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

void EditorController::select_all(const TextDocument& doc) {
    sel_start_line = 0;
    sel_start_col = 0;
    cursor_line = static_cast<int>(doc.lines.size()) - 1;
    cursor_col = static_cast<int>(doc.lines.back().size());
    sel_active = true;
}

bool EditorController::is_identifier_node(TSNode node) const {
    if (ts_node_is_null(node)) return false;
    const char* type = ts_node_type(node);
    return strcmp(type, "identifier") == 0 ||
           strcmp(type, "field_identifier") == 0 ||
           strcmp(type, "type_identifier") == 0 ||
           strcmp(type, "destructor_name") == 0;
}

std::string EditorController::get_node_text(TSNode node, const TextDocument& doc) const {
    if (ts_node_is_null(node)) return "";
    TSPoint start_point = ts_node_start_point(node);
    TSPoint end_point = ts_node_end_point(node);

    if (start_point.row == end_point.row) {
        if (start_point.row < doc.lines.size()) {
            const std::string& line = doc.lines[start_point.row];
            uint32_t start_col = std::min(start_point.column, static_cast<uint32_t>(line.size()));
            uint32_t end_col = std::min(end_point.column, static_cast<uint32_t>(line.size()));
            return line.substr(start_col, end_col - start_col);
        }
        return "";
    }

    std::string result;
    for (uint32_t row = start_point.row; row <= end_point.row && row < doc.lines.size(); row++) {
        const std::string& line = doc.lines[row];
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

TSNode EditorController::get_identifier_at_cursor(const TextDocument& doc, const EditorView& view) {
    if (!view.highlighter.tree) return TSNode{};
    uint32_t byte_offset = doc.get_byte_offset(cursor_pos());
    TSNode root = ts_tree_root_node(view.highlighter.tree.get());
    TSNode node = ts_node_descendant_for_byte_range(root, byte_offset, byte_offset);
    if (is_identifier_node(node)) {
        return node;
    }
    if (cursor_col > 0) {
        uint32_t prev_byte = doc.get_byte_offset({cursor_line, cursor_col - 1});
        node = ts_node_descendant_for_byte_range(root, prev_byte, prev_byte);
        if (is_identifier_node(node)) {
            return node;
        }
    }
    return TSNode{};
}

void EditorController::collect_identifiers_recursive(TSNode node, const std::string& target_name,
                                                      std::vector<HighlightRange>& results, const TextDocument& doc) {
    if (ts_node_is_null(node)) return;
    if (is_identifier_node(node)) {
        std::string name = get_node_text(node, doc);
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
        collect_identifiers_recursive(ts_node_child(node, i), target_name, results, doc);
    }
}

void EditorController::update_highlight_occurrences(const TextDocument& doc, EditorView& view) {
    if (cursor_line == view.last_highlight_line && cursor_col == view.last_highlight_col) {
        return;
    }
    view.last_highlight_line = cursor_line;
    view.last_highlight_col = cursor_col;
    view.highlight_occurrences.clear();
    view.highlighted_identifier.clear();

    if (doc.lines.size() > MAX_LINES_FOR_HIGHLIGHT) {
        return;
    }

    if (!view.highlighter.tree) return;
    TSNode node = get_identifier_at_cursor(doc, view);
    if (ts_node_is_null(node)) return;
    std::string name = get_node_text(node, doc);
    if (name.empty()) return;
    view.highlighted_identifier = name;
    TSNode root = ts_tree_root_node(view.highlighter.tree.get());
    collect_identifiers_recursive(root, name, view.highlight_occurrences, doc);
}

TSNode EditorController::find_name_in_declarator(TSNode declarator, const std::string& target_name, const TextDocument& doc) const {
    if (ts_node_is_null(declarator)) return TSNode{};
    const char* type = ts_node_type(declarator);

    if (is_identifier_node(declarator)) {
        if (get_node_text(declarator, doc) == target_name) return declarator;
        return TSNode{};
    }

    if (strcmp(type, "pointer_declarator") == 0 ||
        strcmp(type, "reference_declarator") == 0 ||
        strcmp(type, "array_declarator") == 0 ||
        strcmp(type, "init_declarator") == 0 ||
        strcmp(type, "parenthesized_declarator") == 0) {
        TSNode child = ts_node_child_by_field_name(declarator, "declarator", 10);
        if (!ts_node_is_null(child)) {
            return find_name_in_declarator(child, target_name, doc);
        }
        uint32_t count = ts_node_child_count(declarator);
        for (uint32_t i = 0; i < count; i++) {
            TSNode res = find_name_in_declarator(ts_node_child(declarator, i), target_name, doc);
            if (!ts_node_is_null(res)) return res;
        }
    }

    if (strcmp(type, "function_declarator") == 0) {
        TSNode child = ts_node_child_by_field_name(declarator, "declarator", 10);
        return find_name_in_declarator(child, target_name, doc);
    }

    if (strcmp(type, "qualified_identifier") == 0) {
        TSNode name_node = ts_node_child_by_field_name(declarator, "name", 4);
        if (!ts_node_is_null(name_node) && get_node_text(name_node, doc) == target_name) {
            return name_node;
        }
    }

    return TSNode{};
}

TSNode EditorController::get_definition_name_node(TSNode node, const std::string& name, const TextDocument& doc) {
    if (ts_node_is_null(node)) return TSNode{};
    const char* type = ts_node_type(node);

    if (strcmp(type, "class_specifier") == 0 ||
        strcmp(type, "struct_specifier") == 0 ||
        strcmp(type, "enum_specifier") == 0 ||
        strcmp(type, "namespace_definition") == 0 ||
        strcmp(type, "class_definition") == 0 ||
        strcmp(type, "class_declaration") == 0) {
        TSNode name_node = ts_node_child_by_field_name(node, "name", 4);
        if (!ts_node_is_null(name_node) && get_node_text(name_node, doc) == name) {
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
            TSNode found = find_name_in_declarator(child, name, doc);
            if (!ts_node_is_null(found)) return found;
        }
    }

    if (strcmp(type, "function_definition") == 0 ||
        strcmp(type, "function_declaration") == 0 ||
        strcmp(type, "method_definition") == 0) {
        TSNode name_node = ts_node_child_by_field_name(node, "name", 4);
        if (!ts_node_is_null(name_node) && get_node_text(name_node, doc) == name) {
            return name_node;
        }
        TSNode declarator = ts_node_child_by_field_name(node, "declarator", 10);
        return find_name_in_declarator(declarator, name, doc);
    }

    if (strcmp(type, "alias_declaration") == 0 || strcmp(type, "type_definition") == 0) {
        uint32_t count = ts_node_child_count(node);
        for (uint32_t i = 0; i < count; i++) {
            TSNode child = ts_node_child(node, i);
            if (strcmp(ts_node_type(child), "type_identifier") == 0 && get_node_text(child, doc) == name) {
                return child;
            }
        }
    }

    if (strcmp(type, "template_parameter_list") == 0) {
        uint32_t count = ts_node_child_count(node);
        for (uint32_t i = 0; i < count; i++) {
            TSNode param = ts_node_child(node, i);
            TSNode found = find_name_in_declarator(param, name, doc);
            if (!ts_node_is_null(found)) return found;
            if (strcmp(ts_node_type(param), "type_parameter_declaration") == 0) {
                TSNode name_node = ts_node_child_by_field_name(param, "name", 4);
                if (!ts_node_is_null(name_node) && get_node_text(name_node, doc) == name) return name_node;
            }
        }
    }

    if (strcmp(type, "assignment") == 0 ||
        strcmp(type, "assignment_statement") == 0) {
        TSNode left = ts_node_child_by_field_name(node, "left", 4);
        if (!ts_node_is_null(left) && get_node_text(left, doc) == name) {
            return left;
        }
    }

    if (strcmp(type, "variable_declarator") == 0 ||
        strcmp(type, "local_variable_declaration") == 0) {
        TSNode name_node = ts_node_child_by_field_name(node, "name", 4);
        if (!ts_node_is_null(name_node) && get_node_text(name_node, doc) == name) {
            return name_node;
        }
    }

    if (strcmp(type, "local_function") == 0 ||
        strcmp(type, "function_statement") == 0) {
        TSNode name_node = ts_node_child_by_field_name(node, "name", 4);
        if (!ts_node_is_null(name_node) && get_node_text(name_node, doc) == name) {
            return name_node;
        }
    }

    if (strcmp(type, "pair") == 0) {
        TSNode key = ts_node_child_by_field_name(node, "key", 3);
        if (!ts_node_is_null(key) && get_node_text(key, doc) == name) {
            return key;
        }
    }

    return TSNode{};
}

TSNode EditorController::find_definition_global(TSNode node, const std::string& name, const TextDocument& doc) {
    if (ts_node_is_null(node)) return TSNode{};

    TSNode def_node = get_definition_name_node(node, name, doc);
    if (!ts_node_is_null(def_node)) return def_node;

    uint32_t count = ts_node_child_count(node);
    for (uint32_t i = 0; i < count; i++) {
        TSNode found = find_definition_global(ts_node_child(node, i), name, doc);
        if (!ts_node_is_null(found)) return found;
    }
    return TSNode{};
}

bool EditorController::go_to_definition(const TextDocument& doc, const EditorView& view) {
    if (!view.highlighter.tree) return false;

    TSNode cursor_node = get_identifier_at_cursor(doc, view);
    if (ts_node_is_null(cursor_node)) return false;

    std::string name = get_node_text(cursor_node, doc);
    if (name.empty()) return false;

    TSNode root = ts_tree_root_node(view.highlighter.tree.get());
    TSNode target_node = TSNode{};

    TSNode current_scope = ts_node_parent(cursor_node);
    while (!ts_node_is_null(current_scope)) {
        uint32_t child_count = ts_node_child_count(current_scope);
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(current_scope, i);
            TSNode def = get_definition_name_node(child, name, doc);
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
        target_node = find_definition_global(root, name, doc);
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
        clear_selection();
        return true;
    }

    return false;
}

void EditorController::set_selection_from_node(TSNode node) {
    TSPoint start = ts_node_start_point(node);
    TSPoint end = ts_node_end_point(node);
    sel_start_line = static_cast<int>(start.row);
    sel_start_col = static_cast<int>(start.column);
    cursor_line = static_cast<int>(end.row);
    cursor_col = static_cast<int>(end.column);
    sel_active = true;
}

bool EditorController::expand_selection(const TextDocument& doc, const EditorView& view) {
    if (!view.highlighter.tree) return false;

    uint32_t current_start_byte, current_end_byte;

    if (has_selection()) {
        auto sel = get_selection_range();
        current_start_byte = doc.get_byte_offset(sel.start);
        current_end_byte = doc.get_byte_offset(sel.end);
    } else {
        current_start_byte = doc.get_byte_offset(cursor_pos());
        current_end_byte = current_start_byte;
        if (selection_stack.empty()) {
            selection_stack.push_back({{{cursor_line, cursor_col}, {cursor_line, cursor_col}}});
        }
    }

    TSNode root = ts_tree_root_node(view.highlighter.tree.get());
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

    SelectionNode new_sel = {{{
        static_cast<int>(start.row), static_cast<int>(start.column)}, {
        static_cast<int>(end.row), static_cast<int>(end.column)
    }}};

    selection_stack.push_back(new_sel);
    set_selection_from_node(node);
    return true;
}

bool EditorController::shrink_selection() {
    if (selection_stack.size() <= 1) {
        clear_selection();
        selection_stack.clear();
        return false;
    }
    selection_stack.pop_back();
    const SelectionNode& prev = selection_stack.back();
    if (prev.range.is_empty()) {
        clear_selection();
        set_cursor_pos(prev.range.start);
    } else {
        sel_start_line = prev.range.start.line;
        sel_start_col = prev.range.start.col;
        set_cursor_pos(prev.range.end);
        sel_active = true;
    }
    return true;
}

void EditorController::reset_selection_stack() {
    selection_stack.clear();
}

bool EditorController::toggle_fold_at_cursor(EditorView& view) {
    LineIdx line = cursor_line;
    if (view.toggle_fold_at_line(line)) {
        return true;
    }
    for (const auto& fr : view.fold_regions) {
        if (cursor_line > fr.start_line && cursor_line <= fr.end_line) {
            if (view.toggle_fold_at_line(fr.start_line)) {
                return true;
            }
        }
    }
    return false;
}

void EditorController::ensure_cursor_not_in_fold(const TextDocument& doc, const EditorView& view) {
    if (!view.is_line_folded(cursor_line)) return;
    for (const auto& fr : view.fold_regions) {
        if (fr.folded && cursor_line > fr.start_line && cursor_line <= fr.end_line) {
            cursor_line = fr.start_line;
            cursor_col = utf8_clamp_to_char_boundary(doc.lines[cursor_line], cursor_col);
            return;
        }
    }
}

void EditorController::update_cursor_from_mouse(int x, int y, int x_offset, int y_offset, TTF_Font* font,
                                                 const TextDocument& doc, const EditorView& view) {
    int text_area_x = x_offset + GUTTER_WIDTH;

    if (x < text_area_x) x = text_area_x;

    int relative_y = y - y_offset;
    if (relative_y < 0) relative_y = 0;

    int lh = view.line_height > 0 ? view.line_height : 20;
    int visual_line_index = relative_y / lh;
    int target_line = view.scroll_y;
    int visual_count = 0;

    while (target_line < static_cast<int>(doc.lines.size()) && visual_count < visual_line_index) {
        if (!view.is_line_folded(target_line)) {
            visual_count++;
        }
        target_line++;
    }

    while (target_line < static_cast<int>(doc.lines.size()) && view.is_line_folded(target_line)) {
        target_line++;
    }

    if (target_line >= static_cast<int>(doc.lines.size())) {
        target_line = static_cast<int>(doc.lines.size()) - 1;
    }

    cursor_line = target_line;

    int text_x = x_offset + GUTTER_WIDTH + PADDING - view.scroll_x;
    int click_x = x - text_x;

    if (click_x <= 0 || doc.lines[cursor_line].empty()) {
        cursor_col = 0;
    } else {
        const std::string& line = doc.lines[cursor_line];
        int best_col = 0;
        int best_diff = click_x;

        size_t col = 0;
        while (col < line.size()) {
            col = utf8_next_char_pos(line, col);
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

void EditorController::handle_mouse_click(int x, int y, int x_offset, int y_offset, int visible_width, int visible_height,
                                          TTF_Font* font, const TextDocument& doc, EditorView& view) {
    if (view.is_point_in_scrollbar(x, y, x_offset, y_offset, visible_width, visible_height)) {
        int thumb_height, thumb_y;
        view.get_scrollbar_metrics(visible_height, view.scaled_scrollbar_min_thumb, thumb_height, thumb_y, doc);

        int relative_y = y - y_offset;

        if (relative_y >= thumb_y && relative_y < thumb_y + thumb_height) {
            view.scrollbar_dragging = true;
            view.scrollbar_drag_offset = relative_y - thumb_y;
        } else {
            int total_visible = view.get_total_visible_lines(doc);
            int visible_lines = visible_height / view.line_height;
            int track_height = visible_height - thumb_height;
            int thumb_center_y = relative_y - thumb_height / 2;
            thumb_center_y = std::max(0, std::min(thumb_center_y, track_height));
            float click_ratio = (track_height > 0) ? static_cast<float>(thumb_center_y) / track_height : 0.0f;
            int target_visible_line = static_cast<int>(click_ratio * (total_visible - visible_lines));
            int target_line = view.get_nth_visible_line_from(0, target_visible_line, doc);
            view.scroll_to_line(target_line, doc);
            view.scrollbar_dragging = true;
            view.scrollbar_drag_offset = thumb_height / 2;
        }
        return;
    }

    clear_selection();
    update_cursor_from_mouse(x, y, x_offset, y_offset, font, doc, view);
    start_selection();
}

void EditorController::handle_mouse_double_click(int x, int y, int x_offset, int y_offset,
                                                  TTF_Font* font, const TextDocument& doc, const EditorView& view) {
    update_cursor_from_mouse(x, y, x_offset, y_offset, font, doc, view);
    select_word_at_cursor(doc);
}

void EditorController::handle_mouse_drag(int x, int y, int x_offset, int y_offset, int visible_width, int visible_height,
                                          TTF_Font* font, const TextDocument& doc, EditorView& view) {
    if (view.scrollbar_dragging) {
        int relative_y = y - y_offset - view.scrollbar_drag_offset;
        int thumb_height, thumb_y_unused;
        view.get_scrollbar_metrics(visible_height, view.scaled_scrollbar_min_thumb, thumb_height, thumb_y_unused, doc);

        int track_height = visible_height - thumb_height;
        float scroll_ratio = (track_height > 0) ? static_cast<float>(relative_y) / track_height : 0.0f;
        scroll_ratio = std::max(0.0f, std::min(1.0f, scroll_ratio));

        int total_visible = view.get_total_visible_lines(doc);
        int visible_lines = visible_height / view.line_height;
        int target_visible_line = static_cast<int>(scroll_ratio * (total_visible - visible_lines));
        int target_line = view.get_nth_visible_line_from(0, target_visible_line, doc);
        view.scroll_to_line(target_line, doc);
        return;
    }

    update_cursor_from_mouse(x, y, x_offset, y_offset, font, doc, view);
}

void EditorController::handle_mouse_up(EditorView& view) {
    view.scrollbar_dragging = false;
    if (sel_active && sel_start_line == cursor_line && sel_start_col == cursor_col) {
        clear_selection();
    }
}

void EditorController::handle_mouse_move(int x, int y, int x_offset, int y_offset, int visible_width, int visible_height, EditorView& view) {
    view.scrollbar_hovered = view.is_point_in_scrollbar(x, y, x_offset, y_offset, visible_width, visible_height);
}

void EditorController::reset_state() {
    cursor_line = 0;
    cursor_col = 0;
    sel_start_line = 0;
    sel_start_col = 0;
    sel_active = false;
    selection_stack.clear();
    command_manager.clear();
    current_group_id = 0;
    in_undo_group = false;
}

EditorController::KeyResult EditorController::handle_key(const SDL_Event& /* event */, int /* visible_lines */, TextDocument& /* doc */, EditorView& /* view */) {
    return {};
}
