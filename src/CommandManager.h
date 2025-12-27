#pragma once

#include "Types.h"
#include <variant>
#include <vector>
#include <string>
#include <cstdint>

struct Editor;

struct InsertOp {
    LineIdx line;
    ColIdx col;
    std::string text;
    LineIdx end_line;
    ColIdx end_col;
    uint64_t group_id;

    void apply(Editor& ed);
    void revert(Editor& ed);
};

struct DeleteOp {
    LineIdx line;
    ColIdx col;
    std::string deleted_text;
    LineIdx end_line;
    ColIdx end_col;
    uint64_t group_id;

    void apply(Editor& ed);
    void revert(Editor& ed);
};

struct MoveLineOp {
    LineIdx block_start;
    LineIdx block_end;
    int direction;
    uint64_t group_id;

    void apply(Editor& ed);
    void revert(Editor& ed);
};

using EditAction = std::variant<InsertOp, DeleteOp, MoveLineOp>;

inline uint64_t get_action_group_id(const EditAction& action) {
    return std::visit([](const auto& op) { return op.group_id; }, action);
}

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

    bool undo(Editor& ed);
    bool redo(Editor& ed);

    bool can_undo() const { return !undo_stack.empty(); }
    bool can_redo() const { return !redo_stack.empty(); }

    void clear() {
        undo_stack.clear();
        redo_stack.clear();
    }
};
