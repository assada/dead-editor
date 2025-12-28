#pragma once

#include "Types.h"
#include "TextDocument.h"
#include "EditorView.h"
#include "CommandManager.h"
#include "Constants.h"
#include "LanguageRegistry.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

class EditorController {
public:
    LineIdx cursor_line = 0;
    ColIdx cursor_col = 0;
    LineIdx sel_start_line = 0;
    ColIdx sel_start_col = 0;
    bool sel_active = false;

    std::vector<SelectionNode> selection_stack;

    CommandManager command_manager{UNDO_HISTORY_MAX};
    uint64_t current_group_id = 0;
    bool in_undo_group = false;

    EditorController() = default;

    TextPos cursor_pos() const { return {cursor_line, cursor_col}; }
    void set_cursor_pos(TextPos pos) { cursor_line = pos.line; cursor_col = pos.col; }

    bool has_selection() const;
    void clear_selection();
    void start_selection();
    TextRange get_selection_range() const;
    std::string get_selected_text(const TextDocument& doc) const;

    void begin_undo_group();
    void end_undo_group();
    uint64_t get_undo_group_id();
    void push_action(EditAction action);
    bool undo(TextDocument& doc, EditorView& view);
    bool redo(TextDocument& doc, EditorView& view);

    void delete_selection(TextDocument& doc, EditorView& view);
    void insert_text(TextDocument& doc, EditorView& view, const char* text);
    void new_line(TextDocument& doc, EditorView& view);
    void backspace(TextDocument& doc, EditorView& view);
    void delete_char(TextDocument& doc, EditorView& view);
    void toggle_comment(TextDocument& doc, EditorView& view);
    void duplicate_line(TextDocument& doc, EditorView& view);

    void move_left(const TextDocument& doc);
    void move_right(const TextDocument& doc);
    void move_up(const TextDocument& doc, const EditorView& view);
    void move_down(const TextDocument& doc, const EditorView& view);
    void move_word_left(const TextDocument& doc);
    void move_word_right(const TextDocument& doc);
    void move_home();
    void move_end(const TextDocument& doc);
    void move_page_up(const TextDocument& doc, int visible_lines);
    void move_page_down(const TextDocument& doc, int visible_lines);
    void move_line_up(TextDocument& doc, EditorView& view);
    void move_line_down(TextDocument& doc, EditorView& view);
    void delete_word_left(TextDocument& doc, EditorView& view);
    void delete_word_right(TextDocument& doc, EditorView& view);

    void go_to(const TextDocument& doc, TextPos pos);
    bool find_next(const TextDocument& doc, const std::string& query, TextPos start);

    bool go_to_definition(const TextDocument& doc, const EditorView& view);
    bool expand_selection(const TextDocument& doc, const EditorView& view);
    bool shrink_selection();
    void reset_selection_stack();

    bool toggle_fold_at_cursor(EditorView& view);
    void ensure_cursor_not_in_fold(const TextDocument& doc, const EditorView& view);

    void select_word_at_cursor(const TextDocument& doc);
    void select_all(const TextDocument& doc);

    void update_highlight_occurrences(const TextDocument& doc, EditorView& view);

    void handle_mouse_click(int x, int y, int x_offset, int y_offset, int visible_width, int visible_height,
                            TTF_Font* font, const TextDocument& doc, EditorView& view);
    void handle_mouse_double_click(int x, int y, int x_offset, int y_offset,
                                   TTF_Font* font, const TextDocument& doc, const EditorView& view);
    void handle_mouse_drag(int x, int y, int x_offset, int y_offset, int visible_width, int visible_height,
                           TTF_Font* font, const TextDocument& doc, EditorView& view);
    void handle_mouse_up(EditorView& view);
    void handle_mouse_move(int x, int y, int x_offset, int y_offset, int visible_width, int visible_height, EditorView& view);

    struct KeyResult {
        bool consumed = false;
        bool cursor_moved = false;
    };
    KeyResult handle_key(const SDL_Event& event, int visible_lines, TextDocument& doc, EditorView& view);

    void reset_state();

    void update_cursor_from_mouse(int x, int y, int x_offset, int y_offset, TTF_Font* font,
                                  const TextDocument& doc, const EditorView& view);

private:
    bool is_word_char_at(const std::string& str, ColIdx pos) const;
    char get_closing_pair(char c, const std::vector<AutoPair>& pairs);
    bool is_closing_char(char c, const std::vector<AutoPair>& pairs);
    char get_opening_pair(char c, const std::vector<AutoPair>& pairs);

    bool is_identifier_node(TSNode node) const;
    std::string get_node_text(TSNode node, const TextDocument& doc) const;
    TSNode get_identifier_at_cursor(const TextDocument& doc, const EditorView& view);
    void collect_identifiers_recursive(TSNode node, const std::string& target_name,
                                       std::vector<HighlightRange>& results, const TextDocument& doc);
    TSNode find_name_in_declarator(TSNode declarator, const std::string& target_name, const TextDocument& doc) const;
    TSNode get_definition_name_node(TSNode node, const std::string& name, const TextDocument& doc);
    TSNode find_definition_global(TSNode node, const std::string& name, const TextDocument& doc);
    void set_selection_from_node(TSNode node);
};
