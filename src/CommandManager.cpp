#include "CommandManager.h"
#include "TextDocument.h"
#include "EditorController.h"

void apply_action(InsertOp& op, TextDocument& doc, EditorController& ctrl) {
    TextPos end_pos;
    doc.insert_at({op.line, op.col}, op.text, end_pos);
    op.end_line = end_pos.line;
    op.end_col = end_pos.col;
    ctrl.cursor_line = end_pos.line;
    ctrl.cursor_col = end_pos.col;
}

void apply_action(DeleteOp& op, TextDocument& doc, EditorController& ctrl) {
    std::string deleted;
    doc.delete_range({op.line, op.col}, {op.end_line, op.end_col}, deleted);
    op.deleted_text = std::move(deleted);
    ctrl.cursor_line = op.line;
    ctrl.cursor_col = op.col;
}

void apply_action(MoveLineOp& op, TextDocument& doc, EditorController& ctrl) {
    doc.move_lines(op.block_start, op.block_end, op.direction);
    ctrl.cursor_line += op.direction;
    if (ctrl.sel_active) {
        ctrl.sel_start_line += op.direction;
    }
}

void revert_action(InsertOp& op, TextDocument& doc, EditorController& ctrl) {
    std::string deleted;
    doc.delete_range({op.line, op.col}, {op.end_line, op.end_col}, deleted);
    ctrl.cursor_line = op.line;
    ctrl.cursor_col = op.col;
}

void revert_action(DeleteOp& op, TextDocument& doc, EditorController& ctrl) {
    TextPos end_pos;
    doc.insert_at({op.line, op.col}, op.deleted_text, end_pos);
    ctrl.cursor_line = end_pos.line;
    ctrl.cursor_col = end_pos.col;
}

void revert_action(MoveLineOp& op, TextDocument& doc, EditorController& ctrl) {
    int new_block_start = op.block_start + op.direction;
    int new_block_end = op.block_end + op.direction;
    doc.move_lines(new_block_start, new_block_end, -op.direction);
    ctrl.cursor_line -= op.direction;
    if (ctrl.sel_active) {
        ctrl.sel_start_line -= op.direction;
    }
}
