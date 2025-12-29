#include "Editor.h"
#include "Utils.h"

Editor::Editor() {
    document.set_tree_edit_callback([this](ByteOff start_byte, ByteOff bytes_removed, ByteOff bytes_added,
                                            TSPoint start_point, TSPoint old_end_point, TSPoint new_end_point) {
        view.highlighter.apply_edit(
            start_byte,
            start_byte + bytes_removed,
            start_byte + bytes_added,
            start_point,
            old_end_point,
            new_end_point
        );
        view.mark_syntax_dirty();
    });
}

bool Editor::load_file(const char* path) {
    auto result = document.load(path);
    if (!result) {
        return false;
    }

    controller.reset_state();
    view.clear_caches();
    view.init_for_file(document.file_path, document);

    return true;
}

void Editor::load_text(const std::string& text) {
    document.load_text(text);
    controller.reset_state();
    view.clear_caches();
    view.init_for_file(document.file_path, document);
}

bool Editor::save_file() {
    if (document.file_path.empty()) {
        std::string new_path = show_save_dialog();
        if (new_path.empty()) {
            return false;
        }
        document.file_path = new_path;
    }

    auto result = document.save();
    return result.has_value();
}

void Editor::render(SDL_Renderer* renderer, TTF_Font* font, TextureCache& texture_cache,
                    const std::string& search_query,
                    int x_offset, int y_offset, int visible_width, int visible_height,
                    int window_w, int char_width,
                    bool has_focus, bool is_file_open, bool cursor_visible,
                    const Layout& layout,
                    std::function<SDL_Color(TokenType)> syntax_color_func) {

    view.update_smooth_scroll(document);
    view.render(renderer, font, texture_cache,
                document,
                controller.cursor_line, controller.cursor_col,
                controller.sel_active, controller.sel_start_line, controller.sel_start_col,
                search_query,
                x_offset, y_offset, visible_width, visible_height,
                window_w, char_width,
                has_focus, is_file_open, cursor_visible,
                layout,
                syntax_color_func);
}

Editor::KeyResult Editor::handle_key(const SDL_Event& event, int visible_lines) {
    auto ctrl_result = controller.handle_key(event, visible_lines, document, view);
    return {ctrl_result.consumed, ctrl_result.cursor_moved};
}
