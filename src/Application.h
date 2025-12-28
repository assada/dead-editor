#pragma once

#include <SDL2/SDL.h>
#include <string>
#include <memory>

#include "HandleTypes.h"
#include "Layout.h"
#include "TextureCache.h"
#include "FontManager.h"
#include "FileTree.h"
#include "Terminal.h"
#include "TabBar.h"
#include "CommandBar.h"
#include "MenuBar.h"
#include "ContextMenu.h"
#include "Types.h"
#include "InputMapper.h"
#include "ActionRegistry.h"
#include "EditorActions.h"
#include "AppActions.h"
#include "FileTreeActions.h"

class Application {
public:
    Application(int argc, char* argv[]);
    ~Application();

    void run();

private:
    void init_systems();
    void init_ui();

    void process_events();
    void update();
    void render();

    void dispatch_key_event(const SDL_Event& event);
    void dispatch_mouse_event(const SDL_Event& event);
    void dispatch_text_input(const SDL_Event& event);
    void handle_window_resize(const SDL_Event& event);

    void handle_command_bar_key(const SDL_Event& event);
    void setup_keybindings();
    void setup_actions();
    InputContext get_current_input_context() const;

    bool action_open_file(const std::string& path);
    void action_open_folder(const std::string& path);
    void action_save_current();
    void action_close_tab(int index);
    void action_create_node(const std::string& base_path, const std::string& name);
    void action_delete_node(const std::string& path);
    void action_rename_node(const std::string& old_path, const std::string& new_name);

    void toggle_terminal();
    void toggle_focus();
    void update_title(const std::string& path = "");
    void ensure_cursor_visible();

    SDL_Color get_syntax_color(TokenType type);
    void on_font_changed();
    void reset_cursor_blink();

    int get_tree_width() const;
    int get_terminal_height() const;
    int get_content_height() const;

    bool running = true;

    WindowPtr window;
    RendererPtr renderer;
    CursorPtr cursor_arrow;
    CursorPtr cursor_resize_ns;
    CursorPtr cursor_resize_ew;

    Layout layout;
    FontManager font_manager;
    TextureCache texture_cache;

    TabBar tab_bar;
    MenuBar menu_bar;
    FileTree file_tree;
    TerminalEmulator terminal;
    CommandBar command_bar;
    ContextMenu context_menu;

    FocusPanel focus = FocusPanel::Editor;
    FocusPanel focus_before_terminal = FocusPanel::Editor;

    bool show_terminal = false;
    int terminal_height = 0;
    int tree_width = 0;

    struct DragState {
        bool terminal = false;
        bool tree = false;
        bool editor = false;
    } dragging;

    bool menu_click_consumed = false;
    bool cursor_moved = false;

    int window_w = WINDOW_WIDTH;
    int window_h = WINDOW_HEIGHT;

    Uint32 last_blink = 0;
    bool cursor_visible = true;

    InputMapper input_mapper;
    ActionRegistry action_registry;
    std::unique_ptr<EditorActions> editor_actions_;
    std::unique_ptr<AppActions> app_actions_;
    std::unique_ptr<FileTreeActions> filetree_actions_;
};
