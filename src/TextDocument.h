#pragma once

#include "Types.h"
#include "LineOffsetTree.h"
#include <vector>
#include <string>
#include <expected>
#include <filesystem>
#include <functional>
#include <tree_sitter/api.h>

class TextDocument {
public:
    std::vector<std::string> lines;
    LineOffsetTree offset_manager;
    std::string file_path;
    bool readonly = false;
    bool modified = false;

    TextDocument();

    std::expected<void, std::string> load(const std::filesystem::path& path);
    std::expected<void, std::string> save();
    std::expected<void, std::string> save_as(const std::filesystem::path& path);
    void load_text(const std::string& text);
    void clear();

    size_t line_count() const { return lines.size(); }
    bool empty() const { return lines.empty() || (lines.size() == 1 && lines[0].empty()); }
    const std::string& get_line(LineIdx idx) const { return lines[idx]; }
    std::string& get_line_mut(LineIdx idx) { return lines[idx]; }

    void rebuild_line_offsets();
    void update_line_offsets(LineIdx start_line, int delta);
    ByteOff get_byte_offset(TextPos pos) const;

    void insert_at(TextPos pos, const std::string& text, TextPos& out_end);
    void delete_range(TextPos start, TextPos end, std::string& out_deleted);
    void move_lines(LineIdx block_start, LineIdx block_end, int direction);

    using TreeEditCallback = std::function<void(ByteOff start_byte, ByteOff bytes_removed, ByteOff bytes_added,
                                                 TSPoint start_point, TSPoint old_end_point, TSPoint new_end_point)>;
    void set_tree_edit_callback(TreeEditCallback callback);

private:
    TreeEditCallback tree_edit_callback;

    void notify_tree_edit(ByteOff start_byte, ByteOff bytes_removed, ByteOff bytes_added,
                          TSPoint start_point, TSPoint old_end_point, TSPoint new_end_point);
};
