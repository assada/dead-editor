#pragma once

#include "TextDocument.h"
#include "EditorView.h"
#include "EditorController.h"
#include "Layout.h"
#include "TextureCache.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <functional>

struct Editor {
    TextDocument document;
    EditorView view;
    EditorController controller;

    Editor();

    std::vector<std::string>& get_lines() { return document.lines; }
    const std::vector<std::string>& get_lines() const { return document.lines; }

    const std::string& get_file_path() const { return document.file_path; }
    void set_file_path(const std::string& path) { document.file_path = path; }

    bool is_readonly() const { return document.readonly; }
    void set_readonly(bool value) { document.readonly = value; }

    bool is_modified() const { return document.modified; }
    void set_modified(bool value) { document.modified = value; }

    int get_line_height() const { return view.line_height; }
    void set_line_height(int value) { view.line_height = value; }

    bool is_syntax_dirty() const { return view.syntax_dirty; }
    void set_syntax_dirty(bool value) { view.syntax_dirty = value; }

    LineIdx get_cursor_line() const { return controller.cursor_line; }
    void set_cursor_line(LineIdx value) { controller.cursor_line = value; }

    ColIdx get_cursor_col() const { return controller.cursor_col; }
    void set_cursor_col(ColIdx value) { controller.cursor_col = value; }

    bool is_scrollbar_dragging() const { return view.scrollbar_dragging; }

    TextPos cursor_pos() const { return controller.cursor_pos(); }
    void set_cursor_pos(TextPos pos) { controller.set_cursor_pos(pos); }

    int get_scroll_x() const { return view.scroll_x; }
    void set_scroll_x(int value) { view.scroll_x = value; }
    int get_scroll_y() const { return view.scroll_y; }
    void set_scroll_y(int value) { view.scroll_y = value; }

    bool has_selection() const { return controller.has_selection(); }
    void clear_selection() { controller.clear_selection(); }
    void start_selection() { controller.start_selection(); }
    TextRange get_selection_range() const { return controller.get_selection_range(); }
    std::string get_selected_text() const { return controller.get_selected_text(document); }
    void delete_selection() { controller.delete_selection(document, view); }

    void mark_modified() {
        document.modified = true;
        view.mark_syntax_dirty();
    }

    void rebuild_syntax() { view.rebuild_syntax(document); }
    const std::vector<Token>& get_line_tokens(size_t line_idx) { return view.get_line_tokens(line_idx); }

    bool undo() { return controller.undo(document, view); }
    bool redo() { return controller.redo(document, view); }
    void begin_undo_group() { controller.begin_undo_group(); }
    void end_undo_group() { controller.end_undo_group(); }

    void insert_text(const char* text) { controller.insert_text(document, view, text); }
    void new_line() { controller.new_line(document, view); }
    void backspace() { controller.backspace(document, view); }
    void delete_char() { controller.delete_char(document, view); }
    void toggle_comment() { controller.toggle_comment(document, view); }
    void duplicate_line() { controller.duplicate_line(document, view); }

    void move_left() { controller.move_left(document); }
    void move_right() { controller.move_right(document); }
    void move_up() { controller.move_up(document, view); }
    void move_down() { controller.move_down(document, view); }
    void move_word_left() { controller.move_word_left(document); }
    void move_word_right() { controller.move_word_right(document); }
    void move_home() { controller.move_home(); }
    void move_end() { controller.move_end(document); }
    void move_page_up(int visible_lines) { controller.move_page_up(document, visible_lines); }
    void move_page_down(int visible_lines) { controller.move_page_down(document, visible_lines); }
    void move_line_up() { controller.move_line_up(document, view); }
    void move_line_down() { controller.move_line_down(document, view); }
    void delete_word_left() { controller.delete_word_left(document, view); }
    void delete_word_right() { controller.delete_word_right(document, view); }

    void go_to(TextPos pos) { controller.go_to(document, pos); }
    bool find_next(const std::string& query, TextPos start) { return controller.find_next(document, query, start); }

    bool go_to_definition() { return controller.go_to_definition(document, view); }
    bool expand_selection() { return controller.expand_selection(document, view); }
    bool shrink_selection() { return controller.shrink_selection(); }
    void reset_selection_stack() { controller.reset_selection_stack(); }

    bool is_line_folded(LineIdx line) const { return view.is_line_folded(line); }
    bool is_fold_start(LineIdx line) const { return view.is_fold_start(line); }
    bool is_fold_start_folded(LineIdx line) const { return view.is_fold_start_folded(line); }
    LineIdx get_fold_end_line(LineIdx start_line) const { return view.get_fold_end_line(start_line); }
    bool toggle_fold_at_line(LineIdx line) { return view.toggle_fold_at_line(line); }
    bool toggle_fold_at_cursor() {
        bool result = controller.toggle_fold_at_cursor(view);
        if (result) controller.ensure_cursor_not_in_fold(document, view);
        return result;
    }
    void fold_all() {
        view.fold_all();
        controller.ensure_cursor_not_in_fold(document, view);
    }
    void unfold_all() { view.unfold_all(); }

    void update_highlight_occurrences() { controller.update_highlight_occurrences(document, view); }

    int get_total_visible_lines() const { return view.get_total_visible_lines(document); }
    int count_visible_lines_between(LineIdx from, LineIdx to) const { return view.count_visible_lines_between(from, to); }
    LineIdx get_nth_visible_line_from(LineIdx start, int n) const { return view.get_nth_visible_line_from(start, n, document); }
    LineIdx get_first_visible_line_from(LineIdx line) const { return view.get_first_visible_line_from(line); }
    LineIdx get_next_visible_line(LineIdx from, int direction) const { return view.get_next_visible_line(from, direction, document); }

    void ensure_visible(int visible_lines) { view.ensure_cursor_visible(controller.cursor_line, visible_lines, document); }
    void ensure_visible_x(int cursor_pixel_x, int visible_width, int margin) { view.ensure_visible_x(cursor_pixel_x, visible_width, margin); }

    bool load_file(const char* path);
    void load_text(const std::string& text);
    bool save_file();

    void handle_mouse_click(int x, int y, int x_offset, int y_offset, int visible_width, int visible_height, TTF_Font* font) {
        controller.handle_mouse_click(x, y, x_offset, y_offset, visible_width, visible_height, font, document, view);
    }
    void handle_mouse_double_click(int x, int y, int x_offset, int y_offset, TTF_Font* font) {
        controller.handle_mouse_double_click(x, y, x_offset, y_offset, font, document, view);
    }
    void handle_mouse_drag(int x, int y, int x_offset, int y_offset, int visible_width, int visible_height, TTF_Font* font) {
        controller.handle_mouse_drag(x, y, x_offset, y_offset, visible_width, visible_height, font, document, view);
    }
    void handle_mouse_up() { controller.handle_mouse_up(view); }
    void handle_mouse_move(int x, int y, int x_offset, int y_offset, int visible_width, int visible_height) {
        controller.handle_mouse_move(x, y, x_offset, y_offset, visible_width, visible_height, view);
    }

    void handle_scroll(float wheel_x, float wheel_y, int char_width, bool shift_held) {
        view.handle_scroll(wheel_x, wheel_y, char_width, shift_held, document);
    }

    void render(SDL_Renderer* renderer, TTF_Font* font, TextureCache& texture_cache,
                const std::string& search_query,
                int x_offset, int y_offset, int visible_width, int visible_height,
                int window_w, int char_width,
                bool has_focus, bool is_file_open, bool cursor_visible,
                const Layout& layout,
                std::function<SDL_Color(TokenType)> syntax_color_func);

    struct KeyResult {
        bool consumed = false;
        bool cursor_moved = false;
    };
    KeyResult handle_key(const SDL_Event& event, int visible_lines);

    SyntaxHighlighter& highlighter() { return view.highlighter; }
    const SyntaxHighlighter& highlighter() const { return view.highlighter; }

    void update_cursor_from_mouse(int x, int y, int x_offset, int y_offset, TTF_Font* font) {
        controller.update_cursor_from_mouse(x, y, x_offset, y_offset, font, document, view);
    }
};
