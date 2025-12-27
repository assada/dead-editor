#include "CommandManager.h"
#include "Editor.h"

void InsertOp::apply(Editor& ed) {
    int out_end_line, out_end_col;
    ed.apply_insert_internal(line, col, text, out_end_line, out_end_col);
    end_line = out_end_line;
    end_col = out_end_col;
}

void InsertOp::revert(Editor& ed) {
    std::string deleted;
    ed.apply_delete_internal(line, col, end_line, end_col, deleted);
}

void DeleteOp::apply(Editor& ed) {
    std::string deleted;
    ed.apply_delete_internal(line, col, end_line, end_col, deleted);
    deleted_text = std::move(deleted);
}

void DeleteOp::revert(Editor& ed) {
    int out_end_line, out_end_col;
    ed.apply_insert_internal(line, col, deleted_text, out_end_line, out_end_col);
}

void MoveLineOp::apply(Editor& ed) {
    ed.move_line_internal(block_start, block_end, direction);
    ed.cursor_line = block_start + direction;
}

void MoveLineOp::revert(Editor& ed) {
    int new_block_start = block_start + direction;
    int new_block_end = block_end + direction;
    ed.move_line_internal(new_block_start, new_block_end, -direction);
    ed.cursor_line = block_start;
}

bool CommandManager::undo(Editor& ed) {
    if (undo_stack.empty()) return false;

    uint64_t group = get_action_group_id(undo_stack.back());

    while (!undo_stack.empty() && get_action_group_id(undo_stack.back()) == group) {
        EditAction action = std::move(undo_stack.back());
        undo_stack.pop_back();

        std::visit([&ed](auto& op) { op.revert(ed); }, action);

        redo_stack.push_back(std::move(action));
    }

    ed.clear_selection();
    return true;
}

bool CommandManager::redo(Editor& ed) {
    if (redo_stack.empty()) return false;

    uint64_t group = get_action_group_id(redo_stack.back());

    while (!redo_stack.empty() && get_action_group_id(redo_stack.back()) == group) {
        EditAction action = std::move(redo_stack.back());
        redo_stack.pop_back();

        std::visit([&ed](auto& op) { op.apply(ed); }, action);

        undo_stack.push_back(std::move(action));
    }

    ed.clear_selection();
    return true;
}
