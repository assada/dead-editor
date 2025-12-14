#pragma once

#include <string>
#include <cstdint>

enum class TokenType {
    Default,
    Keyword,
    Type,
    String,
    Char,
    Comment,
    Number,
    Preprocessor,
    Operator,
    Function,
    Variable
};

struct Token {
    TokenType type;
    int start;
    int end;

    bool operator==(const Token& other) const {
        return type == other.type && start == other.start && end == other.end;
    }
};

enum class FocusPanel { FileTree, Editor, Terminal };

struct FileLocation {
    std::string path;
    int line = 0;
    int col = 0;
};

struct HighlightRange {
    int line;
    int start_col;
    int end_col;
};

struct FoldRegion {
    int start_line;
    int end_line;
    bool folded = false;
};

struct SelectionNode {
    int start_line;
    int start_col;
    int end_line;
    int end_col;
};

enum class EditOperationType {
    Insert,
    Delete,
    MoveLine
};

struct EditOperation {
    EditOperationType type;
    int line;
    int col;
    std::string text;
    int end_line;
    int end_col;
    uint64_t group_id;
};
