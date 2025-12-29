#pragma once

#include "ActionRegistry.h"
#include <functional>

struct AppActionContext {
    std::function<void()> save_current;
    std::function<void()> start_search;
    std::function<void()> start_goto;
    std::function<void(const std::string&, TextPos)> find_next;
    std::function<std::string()> get_search_query;
    std::function<TextPos()> get_cursor_pos;

    std::function<void()> toggle_focus;
    std::function<void()> focus_terminal;
    std::function<void()> toggle_terminal;

    std::function<void()> next_tab;
    std::function<void()> prev_tab;
    std::function<void()> close_active_tab;

    std::function<void()> zoom_in;
    std::function<void()> zoom_out;
    std::function<void()> zoom_reset;

    std::function<void()> terminal_resize_up;
    std::function<void()> terminal_resize_down;
    std::function<void()> terminal_paste;

    std::function<void()> quit;

    std::function<void()> git_commit;
    std::function<void()> scroll_to_source;
};

class AppActions {
public:
    AppActions(ActionRegistry& registry, InputMapper& mapper)
        : registry_(registry), mapper_(mapper) {}

    void register_all(AppActionContext ctx) {
        ctx_ = std::move(ctx);

        registry_.register_action(Actions::App::Save, [this]() -> ActionResult {
            if (ctx_.save_current) ctx_.save_current();
            return {true, false};
        });

        registry_.register_action(Actions::App::Search, [this]() -> ActionResult {
            if (ctx_.start_search) ctx_.start_search();
            return {true, false};
        });

        registry_.register_action(Actions::App::GoToLine, [this]() -> ActionResult {
            if (ctx_.start_goto) ctx_.start_goto();
            return {true, false};
        });

        registry_.register_action(Actions::App::FindNext, [this]() -> ActionResult {
            if (ctx_.find_next && ctx_.get_search_query && ctx_.get_cursor_pos) {
                std::string query = ctx_.get_search_query();
                if (!query.empty()) {
                    TextPos pos = ctx_.get_cursor_pos();
                    int next_col = pos.col + static_cast<int>(query.size());
                    ctx_.find_next(query, {pos.line, next_col});
                    return {true, true};
                }
            }
            return {true, false};
        });

        registry_.register_action(Actions::App::Quit, [this]() -> ActionResult {
            if (ctx_.quit) ctx_.quit();
            return {true, false};
        });

        registry_.register_action(Actions::App::ToggleFocus, [this]() -> ActionResult {
            if (ctx_.toggle_focus) ctx_.toggle_focus();
            return {true, false};
        });

        registry_.register_action(Actions::App::FocusTerminal, [this]() -> ActionResult {
            if (ctx_.focus_terminal) ctx_.focus_terminal();
            return {true, false};
        });

        registry_.register_action(Actions::App::ToggleTerminal, [this]() -> ActionResult {
            if (ctx_.toggle_terminal) ctx_.toggle_terminal();
            return {true, false};
        });

        registry_.register_action(Actions::App::NextTab, [this]() -> ActionResult {
            if (ctx_.next_tab) ctx_.next_tab();
            return {true, true};
        });

        registry_.register_action(Actions::App::PrevTab, [this]() -> ActionResult {
            if (ctx_.prev_tab) ctx_.prev_tab();
            return {true, true};
        });

        registry_.register_action(Actions::App::CloseTab, [this]() -> ActionResult {
            if (ctx_.close_active_tab) ctx_.close_active_tab();
            return {true, false};
        });

        registry_.register_action(Actions::App::ZoomIn, [this]() -> ActionResult {
            if (ctx_.zoom_in) ctx_.zoom_in();
            return {true, false};
        });

        registry_.register_action(Actions::App::ZoomOut, [this]() -> ActionResult {
            if (ctx_.zoom_out) ctx_.zoom_out();
            return {true, false};
        });

        registry_.register_action(Actions::App::ZoomReset, [this]() -> ActionResult {
            if (ctx_.zoom_reset) ctx_.zoom_reset();
            return {true, false};
        });

        registry_.register_action(Actions::App::TerminalResizeUp, [this]() -> ActionResult {
            if (ctx_.terminal_resize_up) ctx_.terminal_resize_up();
            return {true, false};
        });

        registry_.register_action(Actions::App::TerminalResizeDown, [this]() -> ActionResult {
            if (ctx_.terminal_resize_down) ctx_.terminal_resize_down();
            return {true, false};
        });

        registry_.register_action(Actions::App::TerminalPaste, [this]() -> ActionResult {
            if (ctx_.terminal_paste) ctx_.terminal_paste();
            return {true, false};
        });

        registry_.register_action(Actions::Git::Commit, [this]() -> ActionResult {
            if (ctx_.git_commit) ctx_.git_commit();
            return {true, false};
        });

        registry_.register_action(Actions::App::ScrollToSource, [this]() -> ActionResult {
            if (ctx_.scroll_to_source) ctx_.scroll_to_source();
            return {true, false};
        });

        setup_default_bindings();
    }

private:
    void setup_default_bindings() {
        using namespace Actions::App;

        mapper_.bind({SDLK_s, KeyMod::Primary}, Save, InputContext::Editor);
        mapper_.bind({SDLK_f, KeyMod::Primary}, Search, InputContext::Editor);
        mapper_.bind({SDLK_g, KeyMod::Primary}, GoToLine, InputContext::Editor);
        mapper_.bind({SDLK_F3, KeyMod::None}, FindNext, InputContext::Editor);
        mapper_.bind({SDLK_q, KeyMod::Primary}, Quit, InputContext::Global);

        mapper_.bind({SDLK_e, KeyMod::Primary}, ToggleFocus, InputContext::Global);
        mapper_.bind({SDLK_BACKQUOTE, KeyMod::Primary}, FocusTerminal, InputContext::Global);
        mapper_.bind({SDLK_F5, KeyMod::None}, ToggleTerminal, InputContext::Global);

        mapper_.bind({SDLK_TAB, KeyMod::Primary}, NextTab, InputContext::Editor);
        mapper_.bind({SDLK_TAB, KeyMod::PrimaryShift}, PrevTab, InputContext::Editor);
        mapper_.bind({SDLK_F4, KeyMod::Primary}, CloseTab, InputContext::Editor);

        mapper_.bind({SDLK_PLUS, KeyMod::Primary}, ZoomIn, InputContext::Editor);
        mapper_.bind({SDLK_EQUALS, KeyMod::Primary}, ZoomIn, InputContext::Editor);
        mapper_.bind({SDLK_KP_PLUS, KeyMod::Primary}, ZoomIn, InputContext::Editor);
        mapper_.bind({SDLK_MINUS, KeyMod::Primary}, ZoomOut, InputContext::Editor);
        mapper_.bind({SDLK_KP_MINUS, KeyMod::Primary}, ZoomOut, InputContext::Editor);
        mapper_.bind({SDLK_0, KeyMod::Primary}, ZoomReset, InputContext::Editor);
        mapper_.bind({SDLK_KP_0, KeyMod::Primary}, ZoomReset, InputContext::Editor);

        mapper_.bind({SDLK_UP, KeyMod::PrimaryShift}, TerminalResizeUp, InputContext::Terminal);
        mapper_.bind({SDLK_DOWN, KeyMod::PrimaryShift}, TerminalResizeDown, InputContext::Terminal);
        mapper_.bind({SDLK_v, KeyMod::PrimaryShift}, TerminalPaste, InputContext::Terminal);

        mapper_.bind({SDLK_k, KeyMod::Primary}, Actions::Git::Commit, InputContext::Global);
        mapper_.bind({SDLK_F1, KeyMod::Alt}, ScrollToSource, InputContext::Global);
    }

    ActionRegistry& registry_;
    InputMapper& mapper_;
    AppActionContext ctx_;
};
