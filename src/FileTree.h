#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string>
#include <vector>
#include <unordered_set>
#include <mutex>
#include <atomic>

struct TextureCache;

enum class FileTreeAction {
    None,
    OpenFile,
    FocusEditor,
    Exit,
    StartCreate,
    StartDelete
};

struct FileTreeInputResult {
    bool consumed = false;
    FileTreeAction action = FileTreeAction::None;
    std::string path;
    std::string name;
};

struct GitStatus {
    std::unordered_set<std::string> modified;
    std::unordered_set<std::string> added;
    std::unordered_set<std::string> untracked;
};

std::string get_git_branch(const std::string& path);
GitStatus get_git_status(const std::string& path);
bool git_add(const std::string& repo_path, const std::string& file_path);
bool git_unstage(const std::string& repo_path, const std::string& file_path);
bool git_commit(const std::string& repo_path, const std::string& message);
bool git_pull(const std::string& repo_path);
bool git_push(const std::string& repo_path);
bool git_reset_hard(const std::string& repo_path);
bool git_checkout(const std::string& repo_path, const std::string& branch);

struct FileTreeNode {
    std::string name;
    std::string full_path;
    bool is_directory = false;
    bool expanded = false;
    std::vector<FileTreeNode> children;
    int depth = 0;
};

struct FileTree {
    std::string root_path;
    FileTreeNode root;
    std::vector<FileTreeNode*> visible_nodes;
    int selected_index = 0;
    int scroll_offset = 0;
    int context_menu_index = -1;
    bool active = false;
    std::string filter_query;
    std::vector<FileTreeNode*> filtered_nodes;
    std::unordered_set<std::string> expanded_before_filter;
    std::string git_branch;
    std::unordered_set<std::string> git_modified_files;
    std::unordered_set<std::string> git_untracked_files;
    std::unordered_set<std::string> git_added_files;

    std::mutex git_mutex;
    std::atomic<bool> git_refresh_pending{false};
    std::string pending_git_branch;
    std::unordered_set<std::string> pending_git_modified;
    std::unordered_set<std::string> pending_git_untracked;
    std::unordered_set<std::string> pending_git_added;

    std::mutex fs_mutex;
    std::atomic<bool> fs_scan_pending{false};
    std::atomic<bool> fs_needs_refresh{false};
    std::unordered_set<std::string> current_fs_snapshot;
    std::unordered_set<std::string> pending_fs_snapshot;
    Uint32 last_fs_scan_time = 0;
    static constexpr Uint32 FS_SCAN_INTERVAL_MS = 150;

    std::string current_git_branch;
    std::unordered_set<std::string> current_git_modified;
    std::unordered_set<std::string> current_git_untracked;
    std::unordered_set<std::string> current_git_added;
    std::atomic<bool> git_status_changed{false};
    Uint32 last_git_scan_time = 0;
    static constexpr Uint32 GIT_SCAN_INTERVAL_MS = 500;

    void refresh_git_status_async();
    void apply_pending_git_status();
    void check_git_changes();
    bool is_file_modified(const std::string& path) const;
    bool is_file_untracked(const std::string& path) const;
    bool is_file_added(const std::string& path) const;
    bool is_file_staged(const std::string& path) const;
    bool is_git_repo() const;
    void collect_fs_snapshot(const std::string& dir_path, std::unordered_set<std::string>& snapshot);
    void scan_filesystem_async();
    void check_filesystem_changes();
    void apply_filesystem_refresh();
    void collect_expanded_paths(FileTreeNode* node, std::unordered_set<std::string>& paths);
    void restore_expanded_paths(FileTreeNode* node, const std::unordered_set<std::string>& paths);
    void load_directory(const std::string& path);
    void load_children(FileTreeNode& node);
    void rebuild_visible();
    void add_visible_recursive(FileTreeNode* node);
    void toggle_expand();
    void move_up();
    void move_down();
    FileTreeNode* get_selected();
    bool is_filtering() const;
    void set_filter(const std::string& query);
    void clear_filter();
    void clear_filter_and_select(FileTreeNode* node);
    void apply_filter();
    void collect_matching_nodes(FileTreeNode* node, const std::string& lower_query);
    void save_expanded_state();
    void save_expanded_recursive(FileTreeNode* node);
    void restore_expanded_state();
    void restore_expanded_recursive(FileTreeNode* node);
    void expand_path_to_node(FileTreeNode* target);
    bool expand_path_recursive(FileTreeNode* node, const std::string& target_path);
    void select_by_path(const std::string& path);
    void expand_all_for_filter();
    void expand_and_select_path(const std::string& target_path);
    bool expand_path_by_string(FileTreeNode* node, const std::string& target_path);
    void expand_recursive(FileTreeNode* node);
    void ensure_visible(int visible_lines);
    bool is_loaded() const;
    FileTreeInputResult handle_key_event(const SDL_Event& event, int visible_lines, bool file_is_open);
    bool handle_text_input(const char* text);
    bool handle_text_input_key(const SDL_Event& event);
    void handle_mouse_click(int x, int y, int line_height);
    std::string handle_mouse_double_click(int x, int y, int line_height);
    FileTreeNode* get_node_at_position(int y, int line_height);
    int get_index_at_position(int y, int line_height);
    void handle_scroll(int wheel_y, int visible_lines);
    void render(SDL_Renderer* renderer, TTF_Font* font, TextureCache& texture_cache,
                int x, int y, int width, int height,
                int line_height,
                bool has_focus, bool cursor_visible,
                const std::string& current_editor_path);
};
