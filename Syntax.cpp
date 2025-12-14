#include "Syntax.h"
#include <algorithm>
#include <cstring>
#include <cstdio>

void LinesReadContext::set(const std::vector<std::string>& l, const std::vector<uint32_t>& o) {
    lines = &l;
    line_offsets = &o;
    last_line_idx = 0;
}

std::pair<size_t, uint32_t> LinesReadContext::find_line_and_offset(uint32_t byte_index) const {
    if (line_offsets->size() < 2) {
        return {0, byte_index};
    }

    size_t max_line = line_offsets->size() - 2;

    if (last_line_idx <= max_line) {
        uint32_t start = (*line_offsets)[last_line_idx];
        uint32_t end = (*line_offsets)[last_line_idx + 1];
        if (byte_index >= start && byte_index < end) {
            return {last_line_idx, byte_index - start};
        }

        if (last_line_idx + 1 <= max_line) {
            uint32_t next_start = end;
            uint32_t next_end = (*line_offsets)[last_line_idx + 2];
            if (byte_index >= next_start && byte_index < next_end) {
                last_line_idx++;
                return {last_line_idx, byte_index - next_start};
            }
        }
    }

    auto it = std::upper_bound(line_offsets->begin(), line_offsets->end(), byte_index);
    if (it == line_offsets->begin()) {
        last_line_idx = 0;
        return {0, 0};
    }
    --it;
    size_t line_idx = static_cast<size_t>(it - line_offsets->begin());
    last_line_idx = line_idx;
    uint32_t offset_in_line = byte_index - *it;
    return {line_idx, offset_in_line};
}

const char* ts_input_read_callback(
    void* payload, uint32_t byte_index, TSPoint /*position*/, uint32_t* bytes_read
) {
    auto* ctx = static_cast<LinesReadContext*>(payload);
    if (ctx->lines->empty()) {
        *bytes_read = 0;
        return "";
    }

    auto [line_idx, offset_in_line] = ctx->find_line_and_offset(byte_index);

    if (line_idx >= ctx->lines->size()) {
        *bytes_read = 0;
        return "";
    }

    const std::string& line = (*ctx->lines)[line_idx];

    if (offset_in_line < line.size()) {
        *bytes_read = static_cast<uint32_t>(line.size() - offset_in_line);
        return line.data() + offset_in_line;
    }

    if (offset_in_line == line.size()) {
        static const char newline = '\n';
        *bytes_read = 1;
        return &newline;
    }

    *bytes_read = 0;
    return "";
}

SyntaxHighlighter::SyntaxHighlighter() {
    parser = ts_parser_new();
}

SyntaxHighlighter::~SyntaxHighlighter() {
    if (tree) ts_tree_delete(tree);
    if (parser) ts_parser_delete(parser);
}

bool SyntaxHighlighter::set_language_for_file(const std::string& filepath, const std::vector<std::string>& lines, const std::vector<uint32_t>& line_offsets) {
    LanguageRegistry& registry = LanguageRegistry::instance();
    const LanguageDefinition* def = registry.detect_language(filepath);

    if (!def) {
        current_language = nullptr;
        current_language_id.clear();
        return false;
    }

    if (current_language_id == def->id && current_language) {
        return true;
    }

    current_language = registry.get_or_load(def->id);
    if (!current_language) {
        current_language_id.clear();
        return false;
    }

    current_language_id = def->id;
    const TSLanguage* lang = current_language->config.factory();
    if (!lang) {
        current_language = nullptr;
        current_language_id.clear();
        return false;
    }

    if (!ts_parser_set_language(parser, lang)) {
        current_language = nullptr;
        current_language_id.clear();
        return false;
    }

    if (tree) {
        ts_tree_delete(tree);
        tree = nullptr;
    }

    if (!lines.empty()) {
        parse(lines, line_offsets);
    }

    return true;
}

void SyntaxHighlighter::parse(const std::vector<std::string>& lines, const std::vector<uint32_t>& line_offsets) {
    read_context.set(lines, line_offsets);

    TSInput input;
    input.payload = &read_context;
    input.read = ts_input_read_callback;
    input.encoding = TSInputEncodingUTF8;

    if (tree) {
        ts_tree_delete(tree);
    }
    tree = ts_parser_parse(parser, nullptr, input);
}

void SyntaxHighlighter::apply_edit(uint32_t start_byte, uint32_t old_end_byte, uint32_t new_end_byte,
                                    TSPoint start_point, TSPoint old_end_point, TSPoint new_end_point) {
    if (!tree) return;

    TSInputEdit edit = {
        .start_byte = start_byte,
        .old_end_byte = old_end_byte,
        .new_end_byte = new_end_byte,
        .start_point = start_point,
        .old_end_point = old_end_point,
        .new_end_point = new_end_point
    };
    ts_tree_edit(tree, &edit);
}

void SyntaxHighlighter::parse_incremental(const std::vector<std::string>& lines, const std::vector<uint32_t>& line_offsets) {
    read_context.set(lines, line_offsets);

    TSInput input;
    input.payload = &read_context;
    input.read = ts_input_read_callback;
    input.encoding = TSInputEncodingUTF8;

    TSTree* new_tree = ts_parser_parse(parser, tree, input);

    if (new_tree) {
        if (tree) ts_tree_delete(tree);
        tree = new_tree;
    } else {
        if (tree) ts_tree_delete(tree);
        tree = nullptr;
        ts_parser_reset(parser);
        tree = ts_parser_parse(parser, nullptr, input);
    }
}

std::vector<Token> SyntaxHighlighter::get_line_tokens(uint32_t line_start_byte, uint32_t line_end_byte) const {
    std::vector<Token> tokens;
    if (!tree || !current_language || !current_language->query) {
        return tokens;
    }

    TSQueryCursor* cursor = ts_query_cursor_new();
    ts_query_cursor_set_byte_range(cursor, line_start_byte, line_end_byte);
    ts_query_cursor_exec(cursor, current_language->query, ts_tree_root_node(tree));

    TSQueryMatch match;
    while (ts_query_cursor_next_match(cursor, &match)) {
        for (uint16_t i = 0; i < match.capture_count; i++) {
            const TSQueryCapture& capture = match.captures[i];
            uint32_t id = capture.index;

            if (id < current_language->capture_map.size() && current_language->capture_map[id] != TokenType::Default) {
                uint32_t node_start = ts_node_start_byte(capture.node);
                uint32_t node_end = ts_node_end_byte(capture.node);

                int start = static_cast<int>(std::max(node_start, line_start_byte) - line_start_byte);
                int end = static_cast<int>(std::min(node_end, line_end_byte) - line_start_byte);

                if (start < end) {
                    tokens.push_back({current_language->capture_map[id], start, end});
                }
            }
        }
    }

    ts_query_cursor_delete(cursor);

    std::sort(tokens.begin(), tokens.end(), [](const Token& a, const Token& b) {
        if (a.start != b.start) return a.start < b.start;
        return (a.end - a.start) < (b.end - b.start);
    });

    std::vector<Token> resolved;
    for (const auto& tok : tokens) {
        if (resolved.empty()) {
            resolved.push_back(tok);
            continue;
        }
        Token& last = resolved.back();
        if (tok.start >= last.end) {
            resolved.push_back(tok);
        } else if (tok.start == last.start && tok.end <= last.end) {
            last = tok;
        } else if (tok.start > last.start && tok.end <= last.end) {
            Token before = {last.type, last.start, tok.start};
            Token after = {last.type, tok.end, last.end};
            resolved.pop_back();
            if (before.start < before.end) resolved.push_back(before);
            resolved.push_back(tok);
            if (after.start < after.end) resolved.push_back(after);
        } else if (tok.start < last.end && tok.end > last.end) {
            last.end = tok.start;
            if (last.start >= last.end) resolved.pop_back();
            resolved.push_back(tok);
        }
    }

    return resolved;
}

int SyntaxHighlighter::find_line_for_byte_in_range(uint32_t byte_pos, int hint_line, int range_start, int range_end,
                                                    const std::vector<uint32_t>& line_offsets) const {
    if (hint_line >= range_start && hint_line < range_end) {
        if (byte_pos >= line_offsets[hint_line] && byte_pos < line_offsets[hint_line + 1]) {
            return hint_line;
        }
        if (hint_line + 1 < range_end && byte_pos >= line_offsets[hint_line + 1] && byte_pos < line_offsets[hint_line + 2]) {
            return hint_line + 1;
        }
    }

    auto it_start = line_offsets.begin() + range_start;
    auto it_end = line_offsets.begin() + range_end + 1;
    auto it = std::upper_bound(it_start, it_end, byte_pos);
    if (it == it_start) return range_start;
    return static_cast<int>(std::distance(line_offsets.begin(), it)) - 1;
}

void SyntaxHighlighter::get_viewport_tokens(
    int start_line, int end_line,
    const std::vector<uint32_t>& line_offsets,
    const std::vector<std::string>& lines,
    std::unordered_map<int, std::vector<Token>>& result
) const {
    for (auto it = result.begin(); it != result.end(); ) {
        if (it->first < start_line || it->first >= end_line) {
            it = result.erase(it);
        } else {
            it->second.clear();
            ++it;
        }
    }

    if (!tree || !current_language || !current_language->query || start_line < 0 || end_line > static_cast<int>(lines.size())) {
        return;
    }
    if (line_offsets.empty()) return;

    uint32_t vp_start_byte = line_offsets[start_line];
    uint32_t vp_end_byte = (static_cast<size_t>(end_line) < line_offsets.size())
        ? line_offsets[end_line] : line_offsets.back();

    TSQueryCursor* cursor = ts_query_cursor_new();
    ts_query_cursor_set_byte_range(cursor, vp_start_byte, vp_end_byte);
    ts_query_cursor_exec(cursor, current_language->query, ts_tree_root_node(tree));

    int last_hint_line = start_line;

    TSQueryMatch match;
    while (ts_query_cursor_next_match(cursor, &match)) {
        for (uint16_t i = 0; i < match.capture_count; i++) {
            const TSQueryCapture& capture = match.captures[i];
            uint32_t id = capture.index;

            if (id < current_language->capture_map.size() && current_language->capture_map[id] != TokenType::Default) {
                uint32_t node_start = ts_node_start_byte(capture.node);
                uint32_t node_end = ts_node_end_byte(capture.node);

                int token_start_line = find_line_for_byte_in_range(node_start, last_hint_line, start_line, end_line, line_offsets);
                int token_end_line = find_line_for_byte_in_range(node_end > 0 ? node_end - 1 : 0, token_start_line, start_line, end_line, line_offsets);

                last_hint_line = token_start_line;

                int loop_start = std::max(token_start_line, start_line);
                int loop_end = std::min(token_end_line, end_line - 1);

                for (int line_idx = loop_start; line_idx <= loop_end; line_idx++) {
                    uint32_t line_start_abs = line_offsets[line_idx];
                    uint32_t line_end_abs = line_offsets[line_idx + 1] - 1;

                    uint32_t seg_start = std::max(node_start, line_start_abs);
                    uint32_t seg_end = std::min(node_end, line_end_abs + 1);

                    if (seg_start < seg_end) {
                        int col_start = static_cast<int>(seg_start - line_start_abs);
                        int col_end = static_cast<int>(seg_end - line_start_abs);

                        int line_len = static_cast<int>(lines[line_idx].size());
                        if (col_end > line_len) col_end = line_len;
                        if (col_start > line_len) col_start = line_len;

                        if (col_start < col_end) {
                            result[line_idx].push_back({current_language->capture_map[id], col_start, col_end});
                        }
                    }
                }
            }
        }
    }

    ts_query_cursor_delete(cursor);

    static thread_local std::vector<Token> resolved;
    for (auto& [line_idx, tokens] : result) {
        if (tokens.empty()) continue;

        std::sort(tokens.begin(), tokens.end(), [](const Token& a, const Token& b) {
            if (a.start != b.start) return a.start < b.start;
            return a.end < b.end;
        });

        resolved.clear();
        for (const auto& tok : tokens) {
            if (resolved.empty()) {
                resolved.push_back(tok);
                continue;
            }
            Token& last = resolved.back();
            if (tok.start >= last.end) {
                resolved.push_back(tok);
            } else if (tok.start == last.start && tok.end <= last.end) {
                last = tok;
            } else if (tok.start > last.start && tok.end <= last.end) {
                Token before = {last.type, last.start, tok.start};
                Token after = {last.type, tok.end, last.end};
                resolved.pop_back();
                if (before.start < before.end) resolved.push_back(before);
                resolved.push_back(tok);
                if (after.start < after.end) resolved.push_back(after);
            } else if (tok.start < last.end && tok.end > last.end) {
                last.end = tok.start;
                if (last.start >= last.end) resolved.pop_back();
                resolved.push_back(tok);
            }
        }
        tokens = resolved;
    }
}

const std::string& SyntaxHighlighter::get_line_comment_token() const {
    static const std::string empty;
    if (!current_language) return empty;
    return current_language->config.line_comment_token;
}

bool SyntaxHighlighter::has_language() const {
    return current_language != nullptr;
}
