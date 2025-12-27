#pragma once

#include "Types.h"
#include "Syntax.h"
#include "LanguageRegistry.h"
#include "Constants.h"
#include "Layout.h"
#include "TextureCache.h"
#include "LineOffsetTree.h"
#include "CommandManager.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <cstdint>

struct Editor {
    std::vector<std::string> lines;
    LineOffsetTree offset_manager;
    std::string file_path;
    bool readonly = false;
    LineIdx cursor_line = 0;
    ColIdx cursor_col = 0;
    int scroll_y = 0;
    int scroll_x = 0;
    int line_height = 0;
    LineIdx sel_start_line = 0;
    ColIdx sel_start_col = 0;
    bool sel_active = false;
    bool modified = false;
    bool syntax_dirty = true;
    SyntaxHighlighter highlighter;
    std::unordered_map<size_t, std::vector<Token>> token_cache;
    std::unordered_map<LineIdx, std::vector<Token>> viewport_tokens_buffer;
    std::vector<HighlightRange> highlight_occurrences;
    std::string highlighted_identifier;
    LineIdx last_highlight_line = -1;
    ColIdx last_highlight_col = -1;
    std::vector<SelectionNode> selection_stack;
    std::vector<FoldRegion> fold_regions;
    std::unordered_set<LineIdx> folded_lines;

    bool scrollbar_dragging = false;
    int scrollbar_drag_offset = 0;
    bool scrollbar_hovered = false;
    int scaled_scrollbar_width = SCROLLBAR_WIDTH;
    int scaled_scrollbar_min_thumb = SCROLLBAR_MIN_THUMB_HEIGHT;

    CommandManager command_manager{UNDO_HISTORY_MAX};
    uint64_t current_group_id = 0;
    bool in_undo_group = false;
    Uint32 last_edit_time = 0;

    Editor();

    TextPos cursor_pos() const { return {cursor_line, cursor_col}; }
    void set_cursor_pos(TextPos pos) { cursor_line = pos.line; cursor_col = pos.col; }

    void rebuild_line_offsets();
    void update_line_offsets(LineIdx start_line, int delta);
    ByteOff get_byte_offset(TextPos pos) const;
    void perform_tree_edit(ByteOff start_byte, ByteOff bytes_removed, ByteOff bytes_added,
                           TSPoint start_point, TSPoint old_end_point, TSPoint new_end_point);
    void mark_modified();
    void begin_undo_group();
    void end_undo_group();
    uint64_t get_undo_group_id();
    void push_action(EditAction action);
    void apply_insert_internal(LineIdx line, ColIdx col, const std::string& text, LineIdx& out_end_line, ColIdx& out_end_col);
    void apply_delete_internal(LineIdx start_line, ColIdx start_col, LineIdx end_line, ColIdx end_col, std::string& out_deleted);
    void move_line_internal(LineIdx block_start, LineIdx block_end, int direction);
    bool undo();
    bool redo();
    void rebuild_syntax();
    void prefetch_viewport_tokens(LineIdx start_line, int visible_count);
    const std::vector<Token>& get_line_tokens(size_t line_idx);
    bool has_selection() const;
    void clear_selection();
    void start_selection();
    TextRange get_selection_range() const;
    std::string get_selected_text() const;
    void delete_selection();
    void move_page_up(int visible_lines);
    void move_page_down(int visible_lines);
    void go_to(TextPos pos);
    bool save_file();
    char get_closing_pair(char c);
    bool is_closing_char(char c);
    char get_opening_pair(char c);
    void insert_text(const char* text);
    void new_line();
    void backspace();
    void delete_char();
    void toggle_comment();
    void move_left();
    void move_right();
    bool is_word_char_at(const std::string& str, ColIdx pos) const;
    void move_word_left();
    void move_word_right();
    void delete_word_left();
    void delete_word_right();
    void move_up();
    void move_down();
    void move_line_up();
    void move_line_down();
    void move_home();
    void move_end();
    void duplicate_line();
    bool find_next(const std::string& query, TextPos start);
    int count_visible_lines_between(LineIdx from_line, LineIdx to_line) const;
    LineIdx get_nth_visible_line_from(LineIdx start_line, int n) const;
    LineIdx get_first_visible_line_from(LineIdx line) const;
    void ensure_visible(int visible_lines);
    void ensure_visible_x(int cursor_pixel_x, int visible_width, int margin);
    bool load_file(const char* path);
    void load_text(const std::string& text);
    bool is_identifier_node(TSNode node) const;
    std::string get_node_text(TSNode node) const;
    TSNode get_identifier_at_cursor();
    void collect_identifiers_recursive(TSNode node, const std::string& target_name, std::vector<HighlightRange>& results);
    void update_highlight_occurrences();
    TSNode find_name_in_declarator(TSNode declarator, const std::string& target_name) const;
    TSNode get_definition_name_node(TSNode node, const std::string& name);
    TSNode find_definition_global(TSNode node, const std::string& name);
    bool go_to_definition();
    void set_selection_from_node(TSNode node);
    bool expand_selection();
    bool shrink_selection();
    void reset_selection_stack();
    bool is_foldable_node(TSNode node) const;
    void collect_fold_regions_recursive(TSNode node);
    void update_fold_regions();
    void update_folded_lines();
    FoldRegion* get_fold_region_at_line(LineIdx line);
    bool toggle_fold_at_line(LineIdx line);
    bool toggle_fold_at_cursor();
    void fold_all();
    void unfold_all();
    bool is_line_folded(LineIdx line) const;
    bool is_fold_start(LineIdx line) const;
    bool is_fold_start_folded(LineIdx line) const;
    LineIdx get_fold_end_line(LineIdx start_line) const;
    void ensure_cursor_not_in_fold();
    void handle_mouse_click(int x, int y, int x_offset, int y_offset, int visible_width, int visible_height, TTF_Font* font);
    void handle_mouse_double_click(int x, int y, int x_offset, int y_offset, TTF_Font* font);
    void handle_mouse_drag(int x, int y, int x_offset, int y_offset, int visible_width, int visible_height, TTF_Font* font);
    void handle_mouse_up();
    void handle_mouse_move(int x, int y, int x_offset, int y_offset, int visible_width, int visible_height);
    void update_cursor_from_mouse(int x, int y, int x_offset, int y_offset, TTF_Font* font);
    void select_word_at_cursor();
    int get_total_visible_lines() const;
    void get_scrollbar_metrics(int visible_height, int min_thumb_height, int& thumb_height, int& thumb_y) const;
    bool is_point_in_scrollbar(int x, int y, int x_offset, int y_offset, int visible_width, int visible_height) const;
    void scroll_to_line(LineIdx line);
    LineIdx get_next_visible_line(LineIdx from_line, int direction) const;

    void render(SDL_Renderer* renderer, TTF_Font* font, TextureCache& texture_cache,
                const std::string& search_query,
                int x_offset, int y_offset, int visible_width, int visible_height,
                int window_w, int char_width,
                bool has_focus, bool is_file_open, bool cursor_visible,
                const Layout& layout,
                std::function<SDL_Color(TokenType)> syntax_color_func);
    void handle_scroll(int wheel_x, int wheel_y, int char_width, bool shift_held);

    struct KeyResult {
        bool consumed = false;
        bool cursor_moved = false;
    };
    KeyResult handle_key(const SDL_Event& event, int visible_lines);
};
