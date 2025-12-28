#pragma once

#include "Version.h"
#include <string>

namespace HelpContent {

inline std::string get_about_header() {
    std::string version = "DeadEditor " PROJECT_VERSION_FULL;
    int padding = (78 - static_cast<int>(version.length())) / 2;
    std::string padded_version = std::string(padding, ' ') + version + std::string(78 - padding - static_cast<int>(version.length()), ' ');
    return "╔══════════════════════════════════════════════════════════════════════════════╗\n"
           "║" + padded_version + "║\n"
           "╚══════════════════════════════════════════════════════════════════════════════╝";
}

inline const char* ABOUT_BODY = R"(
A lightweight, fast code editor built with SDL2 and Tree-sitter.

FEATURES
────────────────────────────────────────────────────────────────────────────────
  • Syntax highlighting powered by Tree-sitter
  • Multiple language support (C, C++, Python, JavaScript, TypeScript, etc.)
  • File tree with git status integration
  • Integrated terminal emulator
  • Tab-based file management
  • Code folding
  • Smart selection expansion
  • Go to definition (F12)
  • Search and Go to line
  • Find in Files (requires ripgrep)
  • Undo/Redo with grouping
  • Auto-pairing for brackets and quotes

SUPPORTED LANGUAGES
────────────────────────────────────────────────────────────────────────────────
  C, C++, Python, JavaScript, TypeScript, JSON, YAML, TOML,
  HTML, CSS, Lua, Zig, Markdown, Meson, Diff

TECHNOLOGY
────────────────────────────────────────────────────────────────────────────────
  • SDL2 for rendering and input handling
  • SDL2_ttf for font rendering
  • Tree-sitter for parsing and syntax highlighting
  • libvterm for terminal emulation

LICENSE
────────────────────────────────────────────────────────────────────────────────
  MIT License

────────────────────────────────────────────────────────────────────────────────
                         Made with ♥  and lots of coffee
)";

inline std::string get_about() {
    return get_about_header() + ABOUT_BODY;
}

inline const char* KEYMAP = R"(
╔══════════════════════════════════════════════════════════════════════════════╗
║                               DeadEditor Keymap                              ║
╚══════════════════════════════════════════════════════════════════════════════╝

FILE OPERATIONS
────────────────────────────────────────────────────────────────────────────────
  Ctrl+S              Save current file
  Ctrl+F4             Close current tab
  Ctrl+Tab            Next tab
  Ctrl+Shift+Tab      Previous tab
  Ctrl+Q              Exit editor (when no dialogs open)

NAVIGATION
────────────────────────────────────────────────────────────────────────────────
  Arrow Keys          Move cursor
  Ctrl+Left/Right     Move by word
  Home                Move to line start (smart)
  End                 Move to line end
  Page Up/Down        Scroll by page
  Ctrl+G              Go to line:column
  F12                 Go to definition
  F3                  Find next occurrence

SELECTION
────────────────────────────────────────────────────────────────────────────────
  Shift+Arrow         Extend selection
  Shift+Ctrl+Arrow    Extend selection by word
  Shift+Home/End      Extend selection to line start/end
  Shift+Page Up/Down  Extend selection by page
  Ctrl+A              Select all
  Ctrl+W              Expand selection (smart)
  Ctrl+Shift+W        Shrink selection

EDITING
────────────────────────────────────────────────────────────────────────────────
  Ctrl+C              Copy
  Ctrl+X              Cut
  Ctrl+V              Paste
  Ctrl+Z              Undo
  Ctrl+Y              Redo
  Ctrl+Shift+Z        Redo (alternative)
  Ctrl+D              Duplicate line/selection
  Ctrl+/              Toggle line comment
  Alt+Up              Move line(s) up
  Alt+Down            Move line(s) down
  Tab                 Insert 4 spaces
  Ctrl+Backspace      Delete word left
  Ctrl+Delete         Delete word right

SEARCH
────────────────────────────────────────────────────────────────────────────────
  Ctrl+F              Open search bar
  Ctrl+Shift+F        Find in files (project-wide, requires ripgrep)
  Enter               Find next (in search mode)
  F3                  Find next
  Esc                 Close search bar

CODE FOLDING
────────────────────────────────────────────────────────────────────────────────
  Ctrl+Shift+[        Toggle fold at cursor
  Ctrl+Shift+]        Unfold all
  Ctrl+Shift+K        Fold all

VIEW
────────────────────────────────────────────────────────────────────────────────
  Ctrl++              Increase font size
  Ctrl+-              Decrease font size
  Ctrl+0              Reset font size

PANELS
────────────────────────────────────────────────────────────────────────────────
  Ctrl+E              Toggle focus between Editor and File Tree
  Ctrl+`              Focus terminal (when visible)
  F5                  Toggle terminal panel
  Ctrl+Shift+Up       Increase terminal height
  Ctrl+Shift+Down     Decrease terminal height

TERMINAL
────────────────────────────────────────────────────────────────────────────────
  Ctrl+Shift+V        Paste into terminal
  Mouse Wheel         Scroll terminal history

FILE TREE
────────────────────────────────────────────────────────────────────────────────
  Enter               Open file / Toggle folder
  Up/Down or j/k      Navigate
  Left or h           Collapse folder
  Right or l          Expand folder
  Ctrl+N              Create new file/folder
  Delete              Delete file/folder
  Esc                 Focus editor (if file is open)

────────────────────────────────────────────────────────────────────────────────
                              Press Ctrl+F4 to close
)";

}
