#pragma once

#include "Types.h"
#include "Syntax.h"
#include "LanguageRegistry.h"
#include "Constants.h"
#include "Layout.h"
#include "TextureCache.h"
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
    std::vector<uint32_t> line_offsets;
    std::string file_path;
    bool readonly = false;
    int cursor_line = 0;
    int cursor_col = 0;
    int scroll_y = 0;
    int scroll_x = 0;
    int line_height = 0;
    int sel_start_line = 0;
    int sel_start_col = 0;
    bool sel_active = false;
    bool modified = false;
    bool syntax_dirty = true;
    SyntaxHighlighter highlighter;
    std::unordered_map<size_t, std::vector<Token>> token_cache;
    std::unordered_map<int, std::vector<Token>> viewport_tokens_buffer;
    std::vector<HighlightRange> highlight_occurrences;
    std::string highlighted_identifier;
    int last_highlight_line = -1;
    int last_highlight_col = -1;
    std::vector<SelectionNode> selection_stack;
    std::vector<FoldRegion> fold_regions;
    std::unordered_set<int> folded_lines;

    bool scrollbar_dragging = false;
    int scrollbar_drag_offset = 0;
    bool scrollbar_hovered = false;
    int scaled_scrollbar_width = SCROLLBAR_WIDTH;
    int scaled_scrollbar_min_thumb = SCROLLBAR_MIN_THUMB_HEIGHT;

    std::vector<EditOperation> undo_stack;
    std::vector<EditOperation> redo_stack;
    uint64_t current_group_id = 0;
    bool in_undo_group = false;
    Uint32 last_edit_time = 0;

    Editor();

    void rebuild_line_offsets();
    void update_line_offsets(int start_line, int delta);
    uint32_t get_byte_offset(int line, int col) const;
    void perform_tree_edit(uint32_t start_byte, uint32_t bytes_removed, uint32_t bytes_added,
                           TSPoint start_point, TSPoint old_end_point, TSPoint new_end_point);
    void mark_modified();
    void begin_undo_group();
    void end_undo_group();
    uint64_t get_undo_group_id();
    void push_undo(EditOperation op);
    void apply_insert_internal(int line, int col, const std::string& text, int& out_end_line, int& out_end_col);
    void apply_delete_internal(int start_line, int start_col, int end_line, int end_col, std::string& out_deleted);
    bool undo();
    bool redo();
    void rebuild_syntax();
    void prefetch_viewport_tokens(int start_line, int visible_count);
    const std::vector<Token>& get_line_tokens(size_t line_idx);
    bool has_selection() const;
    void clear_selection();
    void start_selection();
    void get_selection_bounds(int& start_line, int& start_col, int& end_line, int& end_col) const;
    std::string get_selected_text() const;
    void delete_selection();
    void move_page_up(int visible_lines);
    void move_page_down(int visible_lines);
    void go_to(int line, int col);
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
    bool is_word_char_at(const std::string& str, int pos) const;
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
    bool find_next(const std::string& query, int start_line, int start_col);
    int count_visible_lines_between(int from_line, int to_line) const;
    int get_nth_visible_line_from(int start_line, int n) const;
    int get_first_visible_line_from(int line) const;
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
    FoldRegion* get_fold_region_at_line(int line);
    bool toggle_fold_at_line(int line);
    bool toggle_fold_at_cursor();
    void fold_all();
    void unfold_all();
    bool is_line_folded(int line) const;
    bool is_fold_start(int line) const;
    bool is_fold_start_folded(int line) const;
    int get_fold_end_line(int start_line) const;
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
    void scroll_to_line(int line);
    int get_next_visible_line(int from_line, int direction) const;

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
