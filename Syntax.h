#pragma once

#include "Types.h"
#include "LanguageRegistry.h"
#include <tree_sitter/api.h>
#include <string>
#include <vector>
#include <unordered_map>

struct LinesReadContext {
    const std::vector<std::string>* lines;
    const std::vector<uint32_t>* line_offsets;
    mutable size_t last_line_idx = 0;

    void set(const std::vector<std::string>& l, const std::vector<uint32_t>& o);
    std::pair<size_t, uint32_t> find_line_and_offset(uint32_t byte_index) const;
};

const char* ts_input_read_callback(
    void* payload, uint32_t byte_index, TSPoint position, uint32_t* bytes_read
);

struct SyntaxHighlighter {
    TSParser* parser = nullptr;
    TSTree* tree = nullptr;
    LoadedLanguage* current_language = nullptr;
    LinesReadContext read_context;
    std::string current_language_id;

    SyntaxHighlighter();
    ~SyntaxHighlighter();

    bool set_language_for_file(const std::string& filepath, const std::vector<std::string>& lines, const std::vector<uint32_t>& line_offsets);
    void parse(const std::vector<std::string>& lines, const std::vector<uint32_t>& line_offsets);
    void apply_edit(uint32_t start_byte, uint32_t old_end_byte, uint32_t new_end_byte,
                    TSPoint start_point, TSPoint old_end_point, TSPoint new_end_point);
    void parse_incremental(const std::vector<std::string>& lines, const std::vector<uint32_t>& line_offsets);
    std::vector<Token> get_line_tokens(uint32_t line_start_byte, uint32_t line_end_byte) const;
    int find_line_for_byte_in_range(uint32_t byte_pos, int hint_line, int range_start, int range_end,
                                    const std::vector<uint32_t>& line_offsets) const;
    void get_viewport_tokens(
        int start_line, int end_line,
        const std::vector<uint32_t>& line_offsets,
        const std::vector<std::string>& lines,
        std::unordered_map<int, std::vector<Token>>& result
    ) const;
    const std::string& get_line_comment_token() const;
    bool has_language() const;
};
