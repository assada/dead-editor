#pragma once

#include "InputMapper.h"
#include <functional>
#include <unordered_map>
#include <string>
#include <string_view>
#include <version>

struct ActionResult {
    bool consumed = false;
    bool cursor_moved = false;
};

class ActionRegistry {
public:
#if defined(__cpp_lib_move_only_function) && __cpp_lib_move_only_function >= 202110L
    using ActionCallback = std::move_only_function<ActionResult()>;
#else
    using ActionCallback = std::function<ActionResult()>; // FUCK YOU, libc++!
#endif

    void register_action(std::string_view id, ActionCallback callback) {
        actions_.insert_or_assign(std::string(id), std::move(callback));
    }

    bool has_action(std::string_view id) const {
        return actions_.contains(std::string(id));
    }

    ActionResult execute(std::string_view id) {
        if (auto it = actions_.find(std::string(id)); it != actions_.end()) {
            return it->second();
        }
        return {};
    }

    ActionResult try_execute(const SDL_KeyboardEvent& event, const InputMapper& mapper, InputContext context) {
        if (auto action_id = mapper.lookup(event, context)) {
            return execute(*action_id);
        }
        return {};
    }

    void clear() { actions_.clear(); }

    void unregister(std::string_view id) {
        actions_.erase(std::string(id));
    }

private:
    std::unordered_map<std::string, ActionCallback> actions_;
};

namespace Actions {
    namespace FileTree {
        constexpr const char* MoveUp = "filetree.move_up";
        constexpr const char* MoveDown = "filetree.move_down";
        constexpr const char* PageUp = "filetree.page_up";
        constexpr const char* PageDown = "filetree.page_down";
        constexpr const char* Home = "filetree.home";
        constexpr const char* End = "filetree.end";
        constexpr const char* Enter = "filetree.enter";
        constexpr const char* Expand = "filetree.expand";
        constexpr const char* Collapse = "filetree.collapse";
        constexpr const char* ToggleExpand = "filetree.toggle_expand";
        constexpr const char* Escape = "filetree.escape";
        constexpr const char* Backspace = "filetree.backspace";
        constexpr const char* Delete = "filetree.delete";
        constexpr const char* Create = "filetree.create";
        constexpr const char* CollapseAll = "filetree.collapse_all";
        constexpr const char* ToggleHidden = "filetree.toggle_hidden";
        constexpr const char* RevealInFileManager = "filetree.reveal_in_file_manager";
    }

    namespace Editor {
        constexpr const char* NewLine = "editor.new_line";
        constexpr const char* Backspace = "editor.backspace";
        constexpr const char* BackspaceWord = "editor.backspace_word";
        constexpr const char* Delete = "editor.delete";
        constexpr const char* DeleteWord = "editor.delete_word";

        constexpr const char* MoveLeft = "editor.move_left";
        constexpr const char* MoveRight = "editor.move_right";
        constexpr const char* MoveUp = "editor.move_up";
        constexpr const char* MoveDown = "editor.move_down";
        constexpr const char* MoveWordLeft = "editor.move_word_left";
        constexpr const char* MoveWordRight = "editor.move_word_right";
        constexpr const char* MoveHome = "editor.move_home";
        constexpr const char* MoveEnd = "editor.move_end";
        constexpr const char* MovePageUp = "editor.move_page_up";
        constexpr const char* MovePageDown = "editor.move_page_down";

        constexpr const char* SelectLeft = "editor.select_left";
        constexpr const char* SelectRight = "editor.select_right";
        constexpr const char* SelectUp = "editor.select_up";
        constexpr const char* SelectDown = "editor.select_down";
        constexpr const char* SelectWordLeft = "editor.select_word_left";
        constexpr const char* SelectWordRight = "editor.select_word_right";
        constexpr const char* SelectHome = "editor.select_home";
        constexpr const char* SelectEnd = "editor.select_end";
        constexpr const char* SelectPageUp = "editor.select_page_up";
        constexpr const char* SelectPageDown = "editor.select_page_down";
        constexpr const char* SelectAll = "editor.select_all";
        constexpr const char* ExpandSelection = "editor.expand_selection";
        constexpr const char* ShrinkSelection = "editor.shrink_selection";

        constexpr const char* MoveLineUp = "editor.move_line_up";
        constexpr const char* MoveLineDown = "editor.move_line_down";

        constexpr const char* Copy = "editor.copy";
        constexpr const char* Cut = "editor.cut";
        constexpr const char* Paste = "editor.paste";
        constexpr const char* Undo = "editor.undo";
        constexpr const char* Redo = "editor.redo";

        constexpr const char* DuplicateLine = "editor.duplicate_line";
        constexpr const char* ToggleComment = "editor.toggle_comment";
        constexpr const char* InsertTab = "editor.insert_tab";

        constexpr const char* GoToDefinition = "editor.goto_definition";
        constexpr const char* ToggleFold = "editor.toggle_fold";
        constexpr const char* FoldAll = "editor.fold_all";
        constexpr const char* UnfoldAll = "editor.unfold_all";
    }

    namespace App {
        constexpr const char* Save = "app.save";
        constexpr const char* Search = "app.search";
        constexpr const char* GoToLine = "app.goto_line";
        constexpr const char* FindNext = "app.find_next";
        constexpr const char* Quit = "app.quit";

        constexpr const char* ToggleFocus = "app.toggle_focus";
        constexpr const char* FocusTerminal = "app.focus_terminal";
        constexpr const char* ToggleTerminal = "app.toggle_terminal";
        constexpr const char* ScrollToSource = "app.scroll_to_source";

        constexpr const char* NextTab = "app.next_tab";
        constexpr const char* PrevTab = "app.prev_tab";
        constexpr const char* CloseTab = "app.close_tab";

        constexpr const char* ZoomIn = "app.zoom_in";
        constexpr const char* ZoomOut = "app.zoom_out";
        constexpr const char* ZoomReset = "app.zoom_reset";

        constexpr const char* TerminalResizeUp = "app.terminal_resize_up";
        constexpr const char* TerminalResizeDown = "app.terminal_resize_down";
        constexpr const char* TerminalPaste = "app.terminal_paste";
    }

    namespace Git {
        constexpr const char* Commit = "git.commit";
        constexpr const char* Pull = "git.pull";
        constexpr const char* Push = "git.push";
        constexpr const char* ResetHard = "git.reset_hard";
        constexpr const char* Checkout = "git.checkout";
    }
}
