#pragma once

#include "ActionRegistry.h"
#include "FileTree.h"
#include <functional>

struct FileTreeActionContext {
    std::function<FileTreeNode*()> get_selected;
    std::function<void(const std::string&)> open_file;
    std::function<void()> focus_editor;
    std::function<void()> quit;
    std::function<void(const std::string&)> start_create;
    std::function<void(const std::string&, const std::string&)> start_delete;
    std::function<std::string()> get_current_editor_path;
};

class FileTreeActions {
public:
    FileTreeActions(ActionRegistry& registry, InputMapper& mapper)
        : registry_(registry), mapper_(mapper) {}

    void register_all(FileTree* tree, std::function<int()> get_visible_lines,
                      std::function<bool()> has_open_file, FileTreeActionContext ctx) {
        tree_ = tree;
        get_visible_lines_ = std::move(get_visible_lines);
        has_open_file_ = std::move(has_open_file);
        ctx_ = std::move(ctx);

        register_navigation_actions();
        register_manipulation_actions();
        setup_default_bindings();
    }

private:
    void register_navigation_actions() {
        registry_.register_action(Actions::FileTree::MoveUp, [this]() -> ActionResult {
            if (!tree_ || !tree_->is_loaded()) return {};
            tree_->move_up();
            tree_->ensure_visible(get_visible_lines_());
            return {true, false};
        });

        registry_.register_action(Actions::FileTree::MoveDown, [this]() -> ActionResult {
            if (!tree_ || !tree_->is_loaded()) return {};
            tree_->move_down();
            tree_->ensure_visible(get_visible_lines_());
            return {true, false};
        });

        registry_.register_action(Actions::FileTree::PageUp, [this]() -> ActionResult {
            if (!tree_ || !tree_->is_loaded()) return {};
            int visible = get_visible_lines_();
            for (int i = 0; i < visible; ++i) tree_->move_up();
            tree_->ensure_visible(visible);
            return {true, false};
        });

        registry_.register_action(Actions::FileTree::PageDown, [this]() -> ActionResult {
            if (!tree_ || !tree_->is_loaded()) return {};
            int visible = get_visible_lines_();
            for (int i = 0; i < visible; ++i) tree_->move_down();
            tree_->ensure_visible(visible);
            return {true, false};
        });

        registry_.register_action(Actions::FileTree::Home, [this]() -> ActionResult {
            if (!tree_ || !tree_->is_loaded()) return {};
            tree_->selected_index = 0;
            tree_->ensure_visible(get_visible_lines_());
            return {true, false};
        });

        registry_.register_action(Actions::FileTree::End, [this]() -> ActionResult {
            if (!tree_ || !tree_->is_loaded()) return {};
            auto& nodes = tree_->is_filtering() ? tree_->filtered_nodes : tree_->visible_nodes;
            tree_->selected_index = static_cast<int>(nodes.size()) - 1;
            tree_->ensure_visible(get_visible_lines_());
            return {true, false};
        });
    }

    void register_manipulation_actions() {
        registry_.register_action(Actions::FileTree::Enter, [this]() -> ActionResult {
            if (!tree_ || !tree_->is_loaded()) return {};
            FileTreeNode* selected = tree_->get_selected();
            if (!selected) return {true, false};

            if (selected->is_directory) {
                if (tree_->is_filtering()) {
                    tree_->clear_filter_and_select(selected);
                }
                tree_->toggle_expand();
            } else {
                if (tree_->is_filtering()) {
                    tree_->clear_filter_and_select(selected);
                }
                if (ctx_.open_file) ctx_.open_file(selected->full_path);
            }
            return {true, true};
        });

        registry_.register_action(Actions::FileTree::Expand, [this]() -> ActionResult {
            if (!tree_ || !tree_->is_loaded() || tree_->is_filtering()) return {};
            FileTreeNode* selected = tree_->get_selected();
            if (selected && selected->is_directory && !selected->expanded) {
                tree_->toggle_expand();
            }
            return {true, false};
        });

        registry_.register_action(Actions::FileTree::Collapse, [this]() -> ActionResult {
            if (!tree_ || !tree_->is_loaded() || tree_->is_filtering()) return {};
            FileTreeNode* selected = tree_->get_selected();
            if (selected && selected->is_directory && selected->expanded) {
                tree_->toggle_expand();
            }
            return {true, false};
        });

        registry_.register_action(Actions::FileTree::ToggleExpand, [this]() -> ActionResult {
            if (!tree_ || !tree_->is_loaded() || tree_->is_filtering()) return {};
            FileTreeNode* selected = tree_->get_selected();
            if (selected && selected->is_directory) {
                tree_->toggle_expand();
            }
            return {true, false};
        });

        registry_.register_action(Actions::FileTree::Escape, [this]() -> ActionResult {
            if (!tree_ || !tree_->is_loaded()) return {};
            if (tree_->is_filtering()) {
                tree_->clear_filter_and_select(nullptr);
            } else if (has_open_file_()) {
                if (ctx_.focus_editor) ctx_.focus_editor();
            }
            return {true, false};
        });

        registry_.register_action(Actions::FileTree::Backspace, [this]() -> ActionResult {
            if (!tree_ || !tree_->is_loaded() || !tree_->is_filtering()) return {};
            std::string q = tree_->filter_query;
            if (!q.empty()) {
                int prev = utf8_prev_char_start(q, static_cast<int>(q.size()));
                q = q.substr(0, prev);
                if (q.empty()) {
                    tree_->clear_filter_and_select(nullptr);
                } else {
                    tree_->set_filter(q);
                }
            }
            return {true, false};
        });

        registry_.register_action(Actions::FileTree::Delete, [this]() -> ActionResult {
            if (!tree_ || !tree_->is_loaded() || tree_->is_filtering()) return {};
            FileTreeNode* selected = tree_->get_selected();
            if (selected && selected->full_path != tree_->root_path) {
                if (ctx_.start_delete) ctx_.start_delete(selected->full_path, selected->name);
            }
            return {true, false};
        });

        registry_.register_action(Actions::FileTree::Create, [this]() -> ActionResult {
            if (!tree_ || !tree_->is_loaded() || tree_->is_filtering()) return {};
            std::string path = tree_->root_path;
            if (FileTreeNode* selected = tree_->get_selected()) {
                path = selected->is_directory
                    ? selected->full_path
                    : std::filesystem::path(selected->full_path).parent_path().string();
            }
            if (ctx_.start_create) ctx_.start_create(path);
            return {true, false};
        });

        registry_.register_action(Actions::FileTree::CollapseAll, [this]() -> ActionResult {
            if (!tree_ || !tree_->is_loaded()) return {};
            tree_->collapse_all();
            return {true, false};
        });

        registry_.register_action(Actions::FileTree::ToggleHidden, [this]() -> ActionResult {
            if (!tree_ || !tree_->is_loaded()) return {};
            tree_->toggle_hidden_files();
            return {true, false};
        });

        registry_.register_action(Actions::FileTree::RevealInFileManager, [this]() -> ActionResult {
            if (!tree_ || !tree_->is_loaded()) return {};
            FileTreeNode* selected = tree_->get_selected();
            if (selected) {
                tree_->reveal_in_file_manager(selected->full_path);
            }
            return {true, false};
        });
    }

    void setup_default_bindings() {
        using namespace Actions::FileTree;

        mapper_.bind({SDLK_UP, KeyMod::None}, MoveUp, InputContext::FileTree);
        mapper_.bind({SDLK_k, KeyMod::None}, MoveUp, InputContext::FileTree);
        mapper_.bind({SDLK_DOWN, KeyMod::None}, MoveDown, InputContext::FileTree);
        mapper_.bind({SDLK_j, KeyMod::None}, MoveDown, InputContext::FileTree);
        mapper_.bind({SDLK_PAGEUP, KeyMod::None}, PageUp, InputContext::FileTree);
        mapper_.bind({SDLK_PAGEDOWN, KeyMod::None}, PageDown, InputContext::FileTree);
        mapper_.bind({SDLK_HOME, KeyMod::None}, Home, InputContext::FileTree);
        mapper_.bind({SDLK_END, KeyMod::None}, End, InputContext::FileTree);

        mapper_.bind({SDLK_RETURN, KeyMod::None}, Enter, InputContext::FileTree);
        mapper_.bind({SDLK_RIGHT, KeyMod::None}, Expand, InputContext::FileTree);
        mapper_.bind({SDLK_l, KeyMod::None}, Expand, InputContext::FileTree);
        mapper_.bind({SDLK_LEFT, KeyMod::None}, Collapse, InputContext::FileTree);
        mapper_.bind({SDLK_h, KeyMod::None}, Collapse, InputContext::FileTree);
        mapper_.bind({SDLK_SPACE, KeyMod::None}, ToggleExpand, InputContext::FileTree);

        mapper_.bind({SDLK_ESCAPE, KeyMod::None}, Escape, InputContext::FileTree);
        mapper_.bind({SDLK_BACKSPACE, KeyMod::None}, Backspace, InputContext::FileTree);
        mapper_.bind({SDLK_DELETE, KeyMod::None}, Delete, InputContext::FileTree);
        mapper_.bind({SDLK_n, KeyMod::Primary}, Create, InputContext::FileTree);
    }

    ActionRegistry& registry_;
    InputMapper& mapper_;
    FileTree* tree_ = nullptr;
    std::function<int()> get_visible_lines_;
    std::function<bool()> has_open_file_;
    FileTreeActionContext ctx_;
};
