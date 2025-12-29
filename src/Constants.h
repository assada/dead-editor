#pragma once

#include <SDL2/SDL.h>

constexpr int WINDOW_WIDTH = 900;
constexpr int WINDOW_HEIGHT = 700;
constexpr int DEFAULT_FONT_SIZE = 18;
constexpr int MIN_FONT_SIZE = 8;
constexpr int MAX_FONT_SIZE = 72;
constexpr int PADDING = 10;
constexpr int FOLD_GUTTER_WIDTH = 20;
constexpr int LINE_NUMBER_WIDTH = 60;
constexpr int GUTTER_WIDTH = FOLD_GUTTER_WIDTH + LINE_NUMBER_WIDTH;
constexpr int CURSOR_BLINK_MS = 530;
constexpr int STATUS_BAR_HEIGHT = 32;
constexpr int FILE_TREE_WIDTH = 250;
constexpr int FILE_TREE_MIN_WIDTH = 100;
constexpr int FILE_TREE_MAX_WIDTH = 600;
constexpr int SEARCH_BAR_HEIGHT = 30;
constexpr int TERMINAL_MIN_HEIGHT = 100;
constexpr int TERMINAL_MAX_HEIGHT = 600;
constexpr int TERMINAL_RESIZE_STEP = 50;

constexpr int SCROLLBAR_WIDTH = 14;
constexpr int SCROLLBAR_MIN_THUMB_HEIGHT = 30;

constexpr bool SMOOTH_SCROLL_ENABLED = true;
constexpr float SCROLL_SENSITIVITY = 1.0f; 
constexpr float MOUSE_SCROLL_SENSITIVITY = 60.0f;
constexpr float SCROLL_FAST_MULTIPLIER = 6.0f;
constexpr float SCROLL_BOOST_THRESHOLD = 0.05f;
constexpr float MOMENTUM_FRICTION = 0.95f; 
constexpr Uint32 MOMENTUM_DELAY_MS = 20; 
constexpr float MIN_LAUNCH_VELOCITY = 26.0f; 
constexpr float VELOCITY_STOP_THRESHOLD = 0.2f;
constexpr float TOUCHPAD_SCROLL_SENSITIVITY = 30.0f; 

constexpr float SCROLL_ACCELERATION = 3.0f;
constexpr float WHEEL_LERP_FACTOR = 0.25f; 
constexpr float WHEEL_SNAP_THRESHOLD = 0.5f;

constexpr int TAB_BAR_HEIGHT = 32;
constexpr int TAB_PADDING = 12;
constexpr int TAB_CLOSE_SIZE = 14;
constexpr int TAB_CLOSE_PADDING = 8;

constexpr int MENU_BAR_HEIGHT = 28;
constexpr int MENU_ITEM_PADDING = 12;
constexpr int MENU_DROPDOWN_WIDTH = 180;
constexpr int MENU_DROPDOWN_ITEM_HEIGHT = 28;

constexpr size_t UNDO_HISTORY_MAX = 10000;
constexpr size_t MAX_LINES_FOR_FOLDING = 10000;
constexpr size_t MAX_LINES_FOR_HIGHLIGHT = 5000;
constexpr size_t LARGE_FILE_LINES = 10000;
constexpr Uint32 SYNTAX_DEBOUNCE_MS = 150;
constexpr int LONG_LINE_THRESHOLD = 1500;

constexpr const char* FONT_NAME = "JetBrainsMonoNLNerdFont-Regular.ttf";
constexpr const char* FONT_SEARCH_PATHS[] = {
    // Bundled font
    "JetBrainsMonoNLNerdFont-Regular.ttf",
    // Linux paths
    "/usr/share/fonts/TTF/JetBrainsMonoNLNerdFont-Regular.ttf",
    "/usr/share/fonts/truetype/jetbrains-mono/JetBrainsMonoNLNerdFont-Regular.ttf",
    "/usr/local/share/fonts/JetBrainsMonoNLNerdFont-Regular.ttf",
    // macOS paths
    "/Library/Fonts/JetBrainsMonoNLNerdFont-Regular.ttf",
    "/System/Library/Fonts/JetBrainsMonoNLNerdFont-Regular.ttf",
    // Common fallback fonts
    "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
    "/System/Library/Fonts/Menlo.ttc",
    "/System/Library/Fonts/Monaco.ttf",
    nullptr
};

namespace Colors {
    constexpr SDL_Color TEXT = {220, 220, 220, 255};
    constexpr SDL_Color LINE_NUM = {100, 100, 100, 255};
    constexpr SDL_Color LINE_NUM_ACTIVE = {180, 180, 180, 255};
    constexpr SDL_Color CURSOR = {255, 200, 50, 255};
    constexpr SDL_Color BG = {25, 25, 30, 255};
    constexpr SDL_Color ACTIVE_LINE = {35, 35, 42, 255};
    constexpr SDL_Color GUTTER = {30, 30, 35, 255};
    constexpr SDL_Color SEARCH_BG = {40, 40, 50, 255};
    constexpr SDL_Color SEARCH_HIGHLIGHT = {255, 200, 50, 80};
    constexpr SDL_Color SELECTION = {70, 130, 180, 150};
    constexpr SDL_Color OCCURRENCE_HIGHLIGHT = {80, 80, 50, 100};
    constexpr SDL_Color FOLD_INDICATOR = {120, 120, 80, 255};

    constexpr SDL_Color SCROLLBAR_BG = {35, 35, 40, 255};
    constexpr SDL_Color SCROLLBAR_THUMB = {70, 70, 80, 255};
    constexpr SDL_Color SCROLLBAR_THUMB_HOVER = {90, 90, 100, 255};
    constexpr SDL_Color SCROLLBAR_THUMB_ACTIVE = {100, 100, 115, 255};

    constexpr SDL_Color SYNTAX_KEYWORD = {198, 120, 221, 255};
    constexpr SDL_Color SYNTAX_TYPE = {86, 182, 194, 255};
    constexpr SDL_Color SYNTAX_STRING = {152, 195, 121, 255};
    constexpr SDL_Color SYNTAX_COMMENT = {92, 99, 112, 255};
    constexpr SDL_Color SYNTAX_NUMBER = {209, 154, 102, 255};
    constexpr SDL_Color SYNTAX_PREPROC = {224, 108, 117, 255};
    constexpr SDL_Color SYNTAX_FUNCTION = {97, 175, 239, 255};
    constexpr SDL_Color SYNTAX_VARIABLE = {229, 192, 123, 255};
    constexpr SDL_Color SYNTAX_PARAMETER = {171, 178, 191, 255};
    constexpr SDL_Color SYNTAX_PROPERTY = {224, 108, 117, 255};
    constexpr SDL_Color SYNTAX_CONSTANT = {86, 182, 194, 255};
    constexpr SDL_Color SYNTAX_NAMESPACE = {229, 192, 123, 255};
    constexpr SDL_Color SYNTAX_ATTRIBUTE = {198, 120, 221, 255};
    constexpr SDL_Color SYNTAX_TAG = {224, 108, 117, 255};
    constexpr SDL_Color SYNTAX_OPERATOR = {171, 178, 191, 255};
    constexpr SDL_Color SYNTAX_PUNCTUATION = {140, 140, 150, 255};
    constexpr SDL_Color SYNTAX_LABEL = {209, 154, 102, 255};
    constexpr SDL_Color GIT_MODIFIED = {229, 192, 123, 255};
    constexpr SDL_Color GIT_STAGED = {152, 195, 121, 255};
    constexpr SDL_Color GIT_UNTRACKED = {224, 108, 117, 255};
    constexpr SDL_Color GIT_IGNORED = {100, 100, 110, 255};
    constexpr SDL_Color GIT_BRANCH = {152, 195, 121, 255};

    constexpr SDL_Color TERMINAL_DEFAULT_FG = {220, 220, 220, 255};
    constexpr SDL_Color TERMINAL_DEFAULT_BG = {18, 18, 22, 255};

    constexpr SDL_Color TOAST_BG = {38, 38, 45, 245};
    constexpr SDL_Color TOAST_BORDER = {55, 55, 65, 255};
    constexpr SDL_Color TOAST_TEXT = {220, 220, 220, 255};
    constexpr SDL_Color TOAST_TEXT_DIM = {160, 160, 170, 255};
    constexpr SDL_Color TOAST_PROGRESS_BG = {30, 30, 35, 255};

    constexpr SDL_Color TOAST_INFO_INDICATOR = {80, 140, 220, 255};
    constexpr SDL_Color TOAST_INFO_ICON = {100, 160, 240, 255};
    constexpr SDL_Color TOAST_SUCCESS_INDICATOR = {80, 180, 100, 255};
    constexpr SDL_Color TOAST_SUCCESS_ICON = {100, 200, 120, 255};
    constexpr SDL_Color TOAST_WARNING_INDICATOR = {220, 180, 80, 255};
    constexpr SDL_Color TOAST_WARNING_ICON = {240, 200, 100, 255};
    constexpr SDL_Color TOAST_ERROR_INDICATOR = {220, 80, 80, 255};
    constexpr SDL_Color TOAST_ERROR_ICON = {240, 100, 100, 255};
}

constexpr SDL_Color TAB_BG_COLOR = {30, 30, 35, 255};
constexpr SDL_Color TAB_ACTIVE_COLOR = {45, 45, 52, 255};
constexpr SDL_Color TAB_HOVER_COLOR = {38, 38, 44, 255};
constexpr SDL_Color TAB_BORDER_COLOR = {55, 55, 65, 255};
constexpr SDL_Color TAB_TEXT_ACTIVE = {220, 220, 220, 255};
constexpr SDL_Color TAB_TEXT_INACTIVE = {140, 140, 150, 255};
constexpr SDL_Color TAB_CLOSE_COLOR = {100, 100, 110, 255};
constexpr SDL_Color TAB_CLOSE_COLOR_HOVER = {220, 220, 220, 255};
constexpr SDL_Color TAB_CLOSE_HOVER_BG = {70, 70, 80, 255};
constexpr SDL_Color TAB_MODIFIED_DOT = {120, 120, 130, 255};
constexpr SDL_Color TAB_ACTIVE_INDICATOR = {80, 140, 200, 255};

constexpr SDL_Color MENU_BAR_BG = {28, 28, 32, 255};
constexpr SDL_Color MENU_ITEM_HOVER = {50, 50, 58, 255};
constexpr SDL_Color MENU_ITEM_ACTIVE = {60, 60, 70, 255};
constexpr SDL_Color MENU_DROPDOWN_BG = {35, 35, 42, 255};
constexpr SDL_Color MENU_DROPDOWN_HOVER = {55, 55, 65, 255};
constexpr SDL_Color MENU_TEXT = {200, 200, 205, 255};
constexpr SDL_Color MENU_TEXT_DIM = {120, 120, 130, 255};
constexpr SDL_Color MENU_DISABLED = {80, 80, 90, 255};
constexpr SDL_Color MENU_SEPARATOR = {60, 60, 70, 255};

#ifdef __APPLE__
constexpr int META_MOD = KMOD_GUI;
#else
constexpr int META_MOD = KMOD_CTRL;
#endif
