#pragma once

#include <string>
#include <cstdint>

using LineIdx = int32_t;
using ColIdx = int32_t;
using ByteOff = uint32_t;

struct TextPos {
    LineIdx line = 0;
    ColIdx col = 0;

    auto operator<=>(const TextPos&) const = default;

    TextPos offset(LineIdx dl, ColIdx dc) const { return {line + dl, col + dc}; }
};

struct TextRange {
    TextPos start;
    TextPos end;

    auto operator<=>(const TextRange&) const = default;

    bool is_empty() const { return start == end; }
    bool contains(TextPos pos) const {
        if (pos.line < start.line || pos.line > end.line) return false;
        if (pos.line == start.line && pos.col < start.col) return false;
        if (pos.line == end.line && pos.col > end.col) return false;
        return true;
    }
};

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
    Variable,
    Parameter,
    Property,
    Constant,
    Namespace,
    Attribute,
    Tag,
    Punctuation,
    Label
};

struct Token {
    TokenType type;
    ColIdx start;
    ColIdx end;

    bool operator==(const Token& other) const {
        return type == other.type && start == other.start && end == other.end;
    }
};

enum class FocusPanel { FileTree, Editor, Terminal };

struct FileLocation {
    std::string path;
    TextPos pos;
};

struct HighlightRange {
    LineIdx line;
    ColIdx start_col;
    ColIdx end_col;
};

struct FoldRegion {
    LineIdx start_line;
    LineIdx end_line;
    bool folded = false;
};

struct SelectionNode {
    TextRange range;
};
