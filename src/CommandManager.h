#pragma once

#include "Types.h"
#include <variant>
#include <vector>
#include <string>
#include <cstdint>

class TextDocument;
class EditorController;

struct InsertOp {
    LineIdx line;
    ColIdx col;
    std::string text;
    LineIdx end_line;
    ColIdx end_col;
    uint64_t group_id;
};

struct DeleteOp {
    LineIdx line;
    ColIdx col;
    std::string deleted_text;
    LineIdx end_line;
    ColIdx end_col;
    uint64_t group_id;
};

struct MoveLineOp {
    LineIdx block_start;
    LineIdx block_end;
    int direction;
    uint64_t group_id;
};

using EditAction = std::variant<InsertOp, DeleteOp, MoveLineOp>;

inline uint64_t get_action_group_id(const EditAction& action) {
    return std::visit([](const auto& op) { return op.group_id; }, action);
}

void apply_action(InsertOp& op, TextDocument& doc, EditorController& ctrl);
void apply_action(DeleteOp& op, TextDocument& doc, EditorController& ctrl);
void apply_action(MoveLineOp& op, TextDocument& doc, EditorController& ctrl);

void revert_action(InsertOp& op, TextDocument& doc, EditorController& ctrl);
void revert_action(DeleteOp& op, TextDocument& doc, EditorController& ctrl);
void revert_action(MoveLineOp& op, TextDocument& doc, EditorController& ctrl);

class CommandManager {
    std::vector<EditAction> undo_stack;
    std::vector<EditAction> redo_stack;
    size_t max_history;

public:
    explicit CommandManager(size_t max_size = 10000) : max_history(max_size) {}

    void push(EditAction action) {
        if (undo_stack.size() >= max_history) {
            undo_stack.erase(undo_stack.begin());
        }
        undo_stack.push_back(std::move(action));
        redo_stack.clear();
    }

    bool can_undo() const { return !undo_stack.empty(); }
    bool can_redo() const { return !redo_stack.empty(); }

    std::vector<EditAction>& get_undo_stack() { return undo_stack; }
    std::vector<EditAction>& get_redo_stack() { return redo_stack; }

    void clear() {
        undo_stack.clear();
        redo_stack.clear();
    }
};
