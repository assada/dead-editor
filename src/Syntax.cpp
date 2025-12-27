#include "Syntax.h"
#include <algorithm>
#include <cstring>
#include <cstdio>

void LinesReadContext::set(const std::vector<std::string>& l, const LineOffsetTree& t) {
    lines = &l;
    offset_tree = &t;
    last_line_idx = 0;
}

std::pair<size_t, ByteOff> LinesReadContext::find_line_and_offset(ByteOff byte_index) const {
    if (offset_tree->empty()) {
        return {0, byte_index};
    }

    size_t max_line = offset_tree->line_count() - 1;

    if (last_line_idx <= max_line) {
        ByteOff start = offset_tree->get_line_start_offset(static_cast<LineIdx>(last_line_idx));
        ByteOff end = offset_tree->get_line_end_offset(static_cast<LineIdx>(last_line_idx));
        if (byte_index >= start && byte_index < end) {
            return {last_line_idx, byte_index - start};
        }

        if (last_line_idx + 1 <= max_line) {
            ByteOff next_start = end;
            ByteOff next_end = offset_tree->get_line_end_offset(static_cast<LineIdx>(last_line_idx + 1));
            if (byte_index >= next_start && byte_index < next_end) {
                last_line_idx++;
                return {last_line_idx, byte_index - next_start};
            }
        }
    }

    LineIdx line_idx = offset_tree->find_line_by_offset(byte_index);
    last_line_idx = static_cast<size_t>(line_idx);
    ByteOff line_start = offset_tree->get_line_start_offset(line_idx);
    return {last_line_idx, byte_index - line_start};
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

SyntaxHighlighter::SyntaxHighlighter() : parser(ts_parser_new()) {}

bool SyntaxHighlighter::set_language_for_file(const std::string& filepath, const std::vector<std::string>& lines, const LineOffsetTree& offset_tree) {
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

    if (!ts_parser_set_language(parser.get(), lang)) {
        current_language = nullptr;
        current_language_id.clear();
        return false;
    }

    tree.reset();

    if (!lines.empty()) {
        parse(lines, offset_tree);
    }

    return true;
}

void SyntaxHighlighter::parse(const std::vector<std::string>& lines, const LineOffsetTree& offset_tree) {
    read_context.set(lines, offset_tree);

    TSInput input;
    input.payload = &read_context;
    input.read = ts_input_read_callback;
    input.encoding = TSInputEncodingUTF8;

    tree.reset(ts_parser_parse(parser.get(), nullptr, input));
}

void SyntaxHighlighter::apply_edit(ByteOff start_byte, ByteOff old_end_byte, ByteOff new_end_byte,
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
    ts_tree_edit(tree.get(), &edit);
}

void SyntaxHighlighter::parse_incremental(const std::vector<std::string>& lines, const LineOffsetTree& offset_tree) {
    read_context.set(lines, offset_tree);

    TSInput input;
    input.payload = &read_context;
    input.read = ts_input_read_callback;
    input.encoding = TSInputEncodingUTF8;

    TSTree* new_tree = ts_parser_parse(parser.get(), tree.get(), input);

    if (new_tree) {
        tree.reset(new_tree);
    } else {
        tree.reset();
        ts_parser_reset(parser.get());
        tree.reset(ts_parser_parse(parser.get(), nullptr, input));
    }
}

std::vector<Token> SyntaxHighlighter::get_line_tokens(ByteOff line_start_byte, ByteOff line_end_byte) const {
    std::vector<Token> tokens;
    if (!tree || !current_language || !current_language->query) {
        return tokens;
    }

    TSQueryCursorPtr cursor(ts_query_cursor_new());
    ts_query_cursor_set_byte_range(cursor.get(), line_start_byte, line_end_byte);
    ts_query_cursor_exec(cursor.get(), current_language->query, ts_tree_root_node(tree.get()));

    TSQueryMatch match;
    while (ts_query_cursor_next_match(cursor.get(), &match)) {
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

LineIdx SyntaxHighlighter::find_line_for_byte_in_range(ByteOff byte_pos, LineIdx hint_line, LineIdx range_start, LineIdx range_end,
                                                    const LineOffsetTree& offset_tree) const {
    if (hint_line >= range_start && hint_line < range_end) {
        ByteOff hint_start = offset_tree.get_line_start_offset(hint_line);
        ByteOff hint_end = offset_tree.get_line_end_offset(hint_line);
        if (byte_pos >= hint_start && byte_pos < hint_end) {
            return hint_line;
        }
        if (hint_line + 1 < range_end) {
            ByteOff next_end = offset_tree.get_line_end_offset(hint_line + 1);
            if (byte_pos >= hint_end && byte_pos < next_end) {
                return hint_line + 1;
            }
        }
    }

    LineIdx line = offset_tree.find_line_by_offset(byte_pos);
    if (line < range_start) return range_start;
    if (line >= range_end) return range_end - 1;
    return line;
}

void SyntaxHighlighter::get_viewport_tokens(
    LineIdx start_line, LineIdx end_line,
    const LineOffsetTree& offset_tree,
    const std::vector<std::string>& lines,
    std::unordered_map<LineIdx, std::vector<Token>>& result
) const {
    for (auto it = result.begin(); it != result.end(); ) {
        if (it->first < start_line || it->first >= end_line) {
            it = result.erase(it);
        } else {
            it->second.clear();
            ++it;
        }
    }

    if (!tree || !current_language || !current_language->query || start_line < 0 || end_line > static_cast<LineIdx>(lines.size())) {
        return;
    }
    if (offset_tree.empty()) return;

    ByteOff vp_start_byte = offset_tree.get_line_start_offset(start_line);
    ByteOff vp_end_byte = (static_cast<size_t>(end_line) < offset_tree.line_count())
        ? offset_tree.get_line_start_offset(end_line) : offset_tree.total_bytes();

    TSQueryCursorPtr cursor(ts_query_cursor_new());
    ts_query_cursor_set_byte_range(cursor.get(), vp_start_byte, vp_end_byte);
    ts_query_cursor_exec(cursor.get(), current_language->query, ts_tree_root_node(tree.get()));

    LineIdx last_hint_line = start_line;

    TSQueryMatch match;
    while (ts_query_cursor_next_match(cursor.get(), &match)) {
        for (uint16_t i = 0; i < match.capture_count; i++) {
            const TSQueryCapture& capture = match.captures[i];
            uint32_t id = capture.index;

            if (id < current_language->capture_map.size() && current_language->capture_map[id] != TokenType::Default) {
                ByteOff node_start = ts_node_start_byte(capture.node);
                ByteOff node_end = ts_node_end_byte(capture.node);

                LineIdx token_start_line = find_line_for_byte_in_range(node_start, last_hint_line, start_line, end_line, offset_tree);
                LineIdx token_end_line = find_line_for_byte_in_range(node_end > 0 ? node_end - 1 : 0, token_start_line, start_line, end_line, offset_tree);

                last_hint_line = token_start_line;

                LineIdx loop_start = std::max(token_start_line, start_line);
                LineIdx loop_end = std::min(token_end_line, end_line - 1);

                for (LineIdx line_idx = loop_start; line_idx <= loop_end; line_idx++) {
                    ByteOff line_start_abs = offset_tree.get_line_start_offset(line_idx);
                    ByteOff line_end_abs = offset_tree.get_line_end_offset(line_idx) - 1;

                    ByteOff seg_start = std::max(node_start, line_start_abs);
                    ByteOff seg_end = std::min(node_end, line_end_abs + 1);

                    if (seg_start < seg_end) {
                        ColIdx col_start = static_cast<ColIdx>(seg_start - line_start_abs);
                        ColIdx col_end = static_cast<ColIdx>(seg_end - line_start_abs);

                        ColIdx line_len = static_cast<ColIdx>(lines[line_idx].size());
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
