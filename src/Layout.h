#pragma once

#include "Constants.h"

struct Layout {
    float scale = 1.0f;

    int padding;
    int status_bar_height;
    int tab_bar_height;
    int search_bar_height;
    int gutter_width;
    int fold_gutter_width;
    int line_number_width;
    int file_tree_width;
    int file_tree_min;
    int file_tree_max;
    int terminal_min;
    int terminal_max;
    int terminal_resize_step;
    int tab_padding;
    int tab_close_size;
    int tab_close_padding;
    int menu_bar_height;
    int menu_item_padding;
    int menu_dropdown_width;
    int menu_dropdown_item_height;
    int scrollbar_width;
    int scrollbar_min_thumb_height;

    Layout() { update(1.0f); }

    void update(float new_scale) {
        scale = new_scale;
        padding = scaled(PADDING);
        status_bar_height = scaled(STATUS_BAR_HEIGHT);
        tab_bar_height = scaled(TAB_BAR_HEIGHT);
        search_bar_height = scaled(SEARCH_BAR_HEIGHT);
        gutter_width = scaled(GUTTER_WIDTH);
        fold_gutter_width = scaled(FOLD_GUTTER_WIDTH);
        line_number_width = scaled(LINE_NUMBER_WIDTH);
        file_tree_width = scaled(FILE_TREE_WIDTH);
        file_tree_min = scaled(FILE_TREE_MIN_WIDTH);
        file_tree_max = scaled(FILE_TREE_MAX_WIDTH);
        terminal_min = scaled(TERMINAL_MIN_HEIGHT);
        terminal_max = scaled(TERMINAL_MAX_HEIGHT);
        terminal_resize_step = scaled(TERMINAL_RESIZE_STEP);
        tab_padding = scaled(TAB_PADDING);
        tab_close_size = scaled(TAB_CLOSE_SIZE);
        tab_close_padding = scaled(TAB_CLOSE_PADDING);
        menu_bar_height = scaled(MENU_BAR_HEIGHT);
        menu_item_padding = scaled(MENU_ITEM_PADDING);
        menu_dropdown_width = scaled(MENU_DROPDOWN_WIDTH);
        menu_dropdown_item_height = scaled(MENU_DROPDOWN_ITEM_HEIGHT);
        scrollbar_width = scaled(SCROLLBAR_WIDTH);
        scrollbar_min_thumb_height = scaled(SCROLLBAR_MIN_THUMB_HEIGHT);
    }

    template<typename T>
    int scaled(T value) const {
        return static_cast<int>(value * scale);
    }

    int mouse_x(int x) const { return static_cast<int>(x * scale); }
    int mouse_y(int y) const { return static_cast<int>(y * scale); }
};
