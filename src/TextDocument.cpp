#include "TextDocument.h"
#include <fstream>
#include <cstdio>

TextDocument::TextDocument() {
    lines.emplace_back("");
    rebuild_line_offsets();
}

std::expected<void, std::string> TextDocument::load(const std::filesystem::path& path) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) {
        return std::unexpected("Failed to open file: " + path.string());
    }

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (fsize < 0) {
        fclose(f);
        return std::unexpected("Failed to determine file size: " + path.string());
    }

    std::string buffer;
    buffer.resize(fsize);
    size_t read_size = fread(buffer.data(), 1, fsize, f);
    fclose(f);
    buffer.resize(read_size);

    lines.clear();
    lines.shrink_to_fit();
    offset_manager.clear();

    lines.reserve(fsize / 40);

    const char* ptr = buffer.data();
    const char* end = ptr + buffer.size();
    const char* line_start = ptr;

    while (ptr < end) {
        if (*ptr == '\n') {
            lines.emplace_back(line_start, ptr - line_start);
            line_start = ptr + 1;
        }
        ptr++;
    }

    if (line_start < end) {
        lines.emplace_back(line_start, end - line_start);
    } else {
        lines.emplace_back("");
    }

    if (lines.empty()) {
        lines.push_back("");
    }

    rebuild_line_offsets();

    file_path = path.string();
    modified = false;

    return {};
}

std::expected<void, std::string> TextDocument::save() {
    if (file_path.empty()) {
        return std::unexpected("No file path set");
    }
    return save_as(file_path);
}

std::expected<void, std::string> TextDocument::save_as(const std::filesystem::path& path) {
    std::ofstream file(path);
    if (!file.is_open()) {
        return std::unexpected("Failed to open file for writing: " + path.string());
    }

    for (size_t i = 0; i < lines.size(); i++) {
        file << lines[i];
        if (i < lines.size() - 1) {
            file << '\n';
        }
    }

    file_path = path.string();
    modified = false;
    return {};
}

void TextDocument::load_text(const std::string& text) {
    lines.clear();
    lines.shrink_to_fit();
    offset_manager.clear();

    const char* ptr = text.data();
    const char* end = ptr + text.size();
    const char* line_start = ptr;

    while (ptr < end) {
        if (*ptr == '\n') {
            lines.emplace_back(line_start, ptr - line_start);
            line_start = ptr + 1;
        }
        ptr++;
    }

    if (line_start < end) {
        lines.emplace_back(line_start, end - line_start);
    } else if (lines.empty() || line_start == end) {
        lines.emplace_back("");
    }

    if (lines.empty()) {
        lines.push_back("");
    }

    rebuild_line_offsets();
    modified = false;
}

void TextDocument::clear() {
    lines.clear();
    lines.emplace_back("");
    offset_manager.clear();
    rebuild_line_offsets();
    file_path.clear();
    modified = false;
}

void TextDocument::rebuild_line_offsets() {
    offset_manager.build_from_lines(lines);
}

void TextDocument::update_line_offsets(LineIdx start_line, int delta) {
    offset_manager.update(start_line, delta);
}

ByteOff TextDocument::get_byte_offset(TextPos pos) const {
    if (pos.line < 0 || pos.line >= static_cast<LineIdx>(offset_manager.line_count())) return 0;
    return offset_manager.get_line_start_offset(pos.line) + static_cast<ByteOff>(pos.col);
}

void TextDocument::insert_at(TextPos pos, const std::string& text, TextPos& out_end) {
    LineIdx current_line = pos.line;
    ColIdx current_col = pos.col;

    uint32_t start_byte = get_byte_offset(pos);
    TSPoint start_point = {static_cast<uint32_t>(current_line), static_cast<uint32_t>(current_col)};
    TSPoint old_end_point = start_point;

    size_t str_pos = 0;
    while (str_pos < text.size()) {
        size_t newline = text.find('\n', str_pos);
        if (newline == std::string::npos) {
            lines[current_line].insert(current_col, text.substr(str_pos));
            current_col += static_cast<int>(text.size() - str_pos);
            break;
        }
        lines[current_line].insert(current_col, text.substr(str_pos, newline - str_pos));
        current_col += static_cast<int>(newline - str_pos);
        std::string remainder = lines[current_line].substr(current_col);
        lines[current_line] = lines[current_line].substr(0, current_col);
        current_line++;
        current_col = 0;
        lines.insert(lines.begin() + current_line, remainder);
        str_pos = newline + 1;
    }

    out_end.line = current_line;
    out_end.col = current_col;

    modified = true;

    bool has_newlines = text.find('\n') != std::string::npos;
    if (has_newlines) {
        rebuild_line_offsets();
    } else {
        update_line_offsets(pos.line, static_cast<int>(text.size()));
    }

    TSPoint new_end_point = {static_cast<uint32_t>(current_line), static_cast<uint32_t>(current_col)};
    notify_tree_edit(start_byte, 0, static_cast<uint32_t>(text.size()), start_point, old_end_point, new_end_point);
}

void TextDocument::delete_range(TextPos start, TextPos end, std::string& out_deleted) {
    LineIdx s_line = start.line;
    ColIdx s_col = start.col;
    LineIdx e_line = end.line;
    ColIdx e_col = end.col;

    if (s_line > e_line || (s_line == e_line && s_col > e_col)) {
        std::swap(s_line, e_line);
        std::swap(s_col, e_col);
    }

    out_deleted.clear();
    for (int i = s_line; i <= e_line; i++) {
        int col_start = (i == s_line) ? s_col : 0;
        int col_end = (i == e_line) ? e_col : static_cast<int>(lines[i].size());
        out_deleted += lines[i].substr(col_start, col_end - col_start);
        if (i < e_line) out_deleted += '\n';
    }

    uint32_t start_byte = get_byte_offset({s_line, s_col});
    uint32_t end_byte = get_byte_offset({e_line, e_col});
    uint32_t bytes_removed = end_byte - start_byte;
    TSPoint start_point = {static_cast<uint32_t>(s_line), static_cast<uint32_t>(s_col)};
    TSPoint old_end_point = {static_cast<uint32_t>(e_line), static_cast<uint32_t>(e_col)};

    if (s_line == e_line) {
        lines[s_line].erase(s_col, e_col - s_col);
        update_line_offsets(s_line, -static_cast<int>(bytes_removed));
    } else {
        std::string new_line_content = lines[s_line].substr(0, s_col) + lines[e_line].substr(e_col);
        lines.erase(lines.begin() + s_line, lines.begin() + e_line + 1);
        lines.insert(lines.begin() + s_line, new_line_content);
        rebuild_line_offsets();
    }

    modified = true;
    notify_tree_edit(start_byte, bytes_removed, 0, start_point, old_end_point, start_point);
}

void TextDocument::move_lines(LineIdx block_start, LineIdx block_end, int direction) {
    LineIdx affected_start = (direction == -1) ? block_start - 1 : block_start;
    LineIdx affected_end = (direction == -1) ? block_end : block_end + 1;

    uint32_t start_byte = offset_manager.get_line_start_offset(affected_start);
    uint32_t end_byte = offset_manager.get_line_start_offset(affected_end + 1);
    uint32_t byte_len = end_byte - start_byte;

    TSPoint start_point = {static_cast<uint32_t>(affected_start), 0};
    TSPoint end_point = {static_cast<uint32_t>(affected_end + 1), 0};

    if (direction == -1) {
        std::string moving_line = std::move(lines[block_start - 1]);
        for (int i = block_start - 1; i < block_end; ++i) {
            lines[i] = std::move(lines[i + 1]);
        }
        lines[block_end] = std::move(moving_line);
    } else {
        std::string moving_line = std::move(lines[block_end + 1]);
        for (int i = block_end + 1; i > block_start; --i) {
            lines[i] = std::move(lines[i - 1]);
        }
        lines[block_start] = std::move(moving_line);
    }

    modified = true;
    rebuild_line_offsets();
    notify_tree_edit(start_byte, byte_len, byte_len, start_point, end_point, end_point);
}

void TextDocument::set_tree_edit_callback(TreeEditCallback callback) {
    tree_edit_callback = std::move(callback);
}

void TextDocument::notify_tree_edit(ByteOff start_byte, ByteOff bytes_removed, ByteOff bytes_added,
                                     TSPoint start_point, TSPoint old_end_point, TSPoint new_end_point) {
    if (tree_edit_callback) {
        tree_edit_callback(start_byte, bytes_removed, bytes_added, start_point, old_end_point, new_end_point);
    }
}
