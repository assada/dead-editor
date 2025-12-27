#pragma once

#include "Types.h"
#include "HandleTypes.h"
#include "LanguageRegistry.h"
#include "LineOffsetTree.h"
#include <string>
#include <vector>
#include <unordered_map>

struct LinesReadContext {
    const std::vector<std::string>* lines = nullptr;
    const LineOffsetTree* offset_tree = nullptr;
    mutable size_t last_line_idx = 0;

    void set(const std::vector<std::string>& l, const LineOffsetTree& t);
    std::pair<size_t, ByteOff> find_line_and_offset(ByteOff byte_index) const;
};

const char* ts_input_read_callback(
    void* payload, uint32_t byte_index, TSPoint position, uint32_t* bytes_read
);

struct SyntaxHighlighter {
    TSParserPtr parser;
    TSTreePtr tree;
    LoadedLanguage* current_language = nullptr;
    LinesReadContext read_context;
    std::string current_language_id;

    SyntaxHighlighter();

    bool set_language_for_file(const std::string& filepath, const std::vector<std::string>& lines, const LineOffsetTree& offset_tree);
    void parse(const std::vector<std::string>& lines, const LineOffsetTree& offset_tree);
    void apply_edit(ByteOff start_byte, ByteOff old_end_byte, ByteOff new_end_byte,
                    TSPoint start_point, TSPoint old_end_point, TSPoint new_end_point);
    void parse_incremental(const std::vector<std::string>& lines, const LineOffsetTree& offset_tree);
    std::vector<Token> get_line_tokens(ByteOff line_start_byte, ByteOff line_end_byte) const;
    LineIdx find_line_for_byte_in_range(ByteOff byte_pos, LineIdx hint_line, LineIdx range_start, LineIdx range_end,
                                    const LineOffsetTree& offset_tree) const;
    void get_viewport_tokens(
        LineIdx start_line, LineIdx end_line,
        const LineOffsetTree& offset_tree,
        const std::vector<std::string>& lines,
        std::unordered_map<LineIdx, std::vector<Token>>& result
    ) const;
    const std::string& get_line_comment_token() const;
    bool has_language() const;
};
