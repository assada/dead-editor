#include "FileTree.h"
#include "HandleTypes.h"
#include "Utils.h"
#include "RenderUtils.h"
#include "Constants.h"
#include "TextureCache.h"
#include <filesystem>
#include <thread>
#include <algorithm>

std::string get_git_branch(const std::string& path) {
    const std::string cmd = "cd \"" + path + "\" && git rev-parse --abbrev-ref HEAD 2>/dev/null";
    PipeHandle pipe(popen(cmd.c_str(), "r"));
    if (!pipe) return "";
    char buffer[256];
    std::string result;
    if (fgets(buffer, sizeof(buffer), pipe.get())) {
        result = buffer;
        if (!result.empty() && result.back() == '\n') {
            result.pop_back();
        }
    }
    return result;
}

std::string decode_git_path(const std::string& path) {
    if (path.empty()) return path;

    std::string input = path;
    if (input.front() == '"' && input.back() == '"' && input.size() >= 2) {
        input = input.substr(1, input.size() - 2);
    }

    std::string result;
    result.reserve(input.size());

    for (size_t i = 0; i < input.size(); ++i) {
        if (input[i] == '\\' && i + 3 < input.size() &&
            input[i + 1] >= '0' && input[i + 1] <= '3' &&
            input[i + 2] >= '0' && input[i + 2] <= '7' &&
            input[i + 3] >= '0' && input[i + 3] <= '7') {
            char c = static_cast<char>(
                (input[i + 1] - '0') * 64 +
                (input[i + 2] - '0') * 8 +
                (input[i + 3] - '0')
            );
            result += c;
            i += 3;
        } else if (input[i] == '\\' && i + 1 < input.size()) {
            switch (input[i + 1]) {
                case 'n': result += '\n'; ++i; break;
                case 't': result += '\t'; ++i; break;
                case '\\': result += '\\'; ++i; break;
                case '"': result += '"'; ++i; break;
                default: result += input[i]; break;
            }
        } else {
            result += input[i];
        }
    }
    return result;
}

GitStatus get_git_status(const std::string& path) {
    GitStatus status;
    const std::string cmd = "cd \"" + path + "\" && git status --porcelain --ignored 2>/dev/null";
    PipeHandle pipe(popen(cmd.c_str(), "r"));
    if (!pipe) return status;

    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), pipe.get())) {
        std::string line(buffer);
        if (line.size() < 4) continue;

        const char index_status = line[0];
        const char worktree_status = line[1];
        std::string file = line.substr(3);
        if (!file.empty() && file.back() == '\n') file.pop_back();

        if (const size_t arrow_pos = file.find(" -> "); arrow_pos != std::string::npos) {
            file = file.substr(arrow_pos + 4);
        }

        file = decode_git_path(file);
        std::string full_path = path + "/" + file;
        if (full_path.back() == '/') full_path.pop_back();

        if (index_status == '!' && worktree_status == '!') {
            status.ignored.insert(full_path);
        } else if (index_status == '?' && worktree_status == '?') {
            status.untracked.insert(full_path);
        } else if (index_status != ' ' && index_status != '?') {
            status.staged.insert(full_path);
        } else if (worktree_status == 'M' || worktree_status == 'D') {
            status.modified.insert(full_path);
        }
    }
    return status;
}

bool git_add(const std::string& repo_path, const std::string& file_path) {
    const std::string cmd = "cd \"" + repo_path + "\" && git add \"" + file_path + "\" 2>/dev/null";
    return system(cmd.c_str()) == 0;
}

bool git_unstage(const std::string& repo_path, const std::string& file_path) {
    const std::string cmd = "cd \"" + repo_path + "\" && git restore --staged \"" + file_path + "\" 2>/dev/null";
    return system(cmd.c_str()) == 0;
}

bool git_commit(const std::string& repo_path, const std::string& message) {
    std::string escaped_msg;
    for (char c : message) {
        if (c == '"' || c == '\\' || c == '$' || c == '`') {
            escaped_msg += '\\';
        }
        escaped_msg += c;
    }
    const std::string cmd = "cd \"" + repo_path + "\" && git commit -m \"" + escaped_msg + "\" 2>/dev/null";
    return system(cmd.c_str()) == 0;
}

bool git_pull(const std::string& repo_path) {
    const std::string cmd = "cd \"" + repo_path + "\" && git pull 2>/dev/null";
    return system(cmd.c_str()) == 0;
}

bool git_push(const std::string& repo_path) {
    const std::string cmd = "cd \"" + repo_path + "\" && git push 2>/dev/null";
    return system(cmd.c_str()) == 0;
}

bool git_reset_hard(const std::string& repo_path) {
    const std::string cmd = "cd \"" + repo_path + "\" && git reset --hard HEAD 2>/dev/null";
    return system(cmd.c_str()) == 0;
}

bool git_checkout(const std::string& repo_path, const std::string& branch) {
    const std::string cmd = "cd \"" + repo_path + "\" && git checkout \"" + branch + "\" 2>/dev/null";
    return system(cmd.c_str()) == 0;
}

void FileTree::refresh_git_status_async() {
    if (root_path.empty() || git_refresh_pending.load()) return;
    git_refresh_pending.store(true);
    std::string path_copy = root_path;
    std::thread([this, path_copy]() {
        std::string branch = get_git_branch(path_copy);
        GitStatus status = get_git_status(path_copy);
        {
            std::lock_guard<std::mutex> lock(git_mutex);
            pending_git_branch = std::move(branch);
            pending_git_staged = std::move(status.staged);
            pending_git_modified = std::move(status.modified);
            pending_git_untracked = std::move(status.untracked);
            pending_git_ignored = std::move(status.ignored);
        }
        git_refresh_pending.store(false);
    }).detach();
}

void FileTree::apply_pending_git_status() {
    if (git_refresh_pending.load()) return;
    std::lock_guard<std::mutex> lock(git_mutex);
    bool has_pending = !pending_git_branch.empty() || !pending_git_staged.empty() ||
                       !pending_git_modified.empty() || !pending_git_untracked.empty() ||
                       !pending_git_ignored.empty();
    if (!has_pending) return;

    bool changed = (pending_git_branch != current_git_branch) ||
                   (pending_git_staged != current_git_staged) ||
                   (pending_git_modified != current_git_modified) ||
                   (pending_git_untracked != current_git_untracked) ||
                   (pending_git_ignored != current_git_ignored);

    if (changed) {
        current_git_branch = pending_git_branch;
        current_git_staged = pending_git_staged;
        current_git_modified = pending_git_modified;
        current_git_untracked = pending_git_untracked;
        current_git_ignored = pending_git_ignored;
        git_status_changed.store(true);
    }

    git_branch = std::move(pending_git_branch);
    git_staged_files = std::move(pending_git_staged);
    git_modified_files = std::move(pending_git_modified);
    git_untracked_files = std::move(pending_git_untracked);
    git_ignored_files = std::move(pending_git_ignored);
}

void FileTree::check_git_changes() {
    Uint32 now = SDL_GetTicks();
    if (now - last_git_scan_time >= GIT_SCAN_INTERVAL_MS) {
        last_git_scan_time = now;
        if (!git_refresh_pending.load()) {
            refresh_git_status_async();
        }
    }
}

bool FileTree::is_file_staged(const std::string& path) const {
    return git_staged_files.find(path) != git_staged_files.end();
}

bool FileTree::is_file_modified(const std::string& path) const {
    return git_modified_files.find(path) != git_modified_files.end();
}

bool FileTree::is_file_untracked(const std::string& path) const {
    return git_untracked_files.find(path) != git_untracked_files.end();
}

bool FileTree::is_file_ignored(const std::string& path) const {
    if (git_ignored_files.find(path) != git_ignored_files.end()) {
        return true;
    }
    std::filesystem::path p(path);
    while (p.has_parent_path() && p.string() != root_path) {
        p = p.parent_path();
        if (git_ignored_files.find(p.string()) != git_ignored_files.end()) {
            return true;
        }
    }
    return false;
}

bool FileTree::is_root_node(const FileTreeNode* node) const {
    return node && node->full_path == root_path;
}

bool FileTree::is_git_repo() const {
    return !git_branch.empty();
}

void FileTree::collect_fs_snapshot(const std::string& dir_path, std::unordered_set<std::string>& snapshot) {
    try {
        for (const auto& entry : std::filesystem::directory_iterator(dir_path)) {
            snapshot.insert(entry.path().string());
            if (entry.is_directory()) {
                collect_fs_snapshot(entry.path().string(), snapshot);
            }
        }
    } catch (...) {}
}

void FileTree::scan_filesystem_async() {
    if (root_path.empty() || fs_scan_pending.load()) return;
    fs_scan_pending.store(true);
    std::string path_copy = root_path;
    std::thread([this, path_copy]() {
        std::unordered_set<std::string> new_snapshot;
        new_snapshot.insert(path_copy);
        collect_fs_snapshot(path_copy, new_snapshot);
        {
            std::lock_guard<std::mutex> lock(fs_mutex);
            pending_fs_snapshot = std::move(new_snapshot);
        }
        fs_scan_pending.store(false);
    }).detach();
}

void FileTree::check_filesystem_changes() {
    Uint32 now = SDL_GetTicks();
    if (now - last_fs_scan_time >= FS_SCAN_INTERVAL_MS) {
        last_fs_scan_time = now;
        if (!fs_scan_pending.load()) {
            std::lock_guard<std::mutex> lock(fs_mutex);
            if (!pending_fs_snapshot.empty() && pending_fs_snapshot != current_fs_snapshot) {
                current_fs_snapshot = pending_fs_snapshot;
                fs_needs_refresh.store(true);
            }
            pending_fs_snapshot.clear();
        }
        scan_filesystem_async();
    }
}

void FileTree::apply_filesystem_refresh() {
    if (!fs_needs_refresh.load()) return;
    fs_needs_refresh.store(false);

    std::unordered_set<std::string> expanded_paths;
    collect_expanded_paths(&root, expanded_paths);
    std::string selected_path;
    auto& nodes = is_filtering() ? filtered_nodes : visible_nodes;
    if (selected_index >= 0 && selected_index < static_cast<int>(nodes.size())) {
        selected_path = nodes[selected_index]->full_path;
    }

    root.children.clear();
    load_children(root);
    restore_expanded_paths(&root, expanded_paths);
    rebuild_visible();

    if (!selected_path.empty()) {
        auto& updated_nodes = is_filtering() ? filtered_nodes : visible_nodes;
        for (int i = 0; i < static_cast<int>(updated_nodes.size()); i++) {
            if (updated_nodes[i]->full_path == selected_path) {
                selected_index = i;
                break;
            }
        }
    }

    refresh_git_status_async();
}

void FileTree::collect_expanded_paths(FileTreeNode* node, std::unordered_set<std::string>& paths) {
    if (node->is_directory && node->expanded) {
        paths.insert(node->full_path);
    }
    for (auto& child : node->children) {
        collect_expanded_paths(&child, paths);
    }
}

void FileTree::restore_expanded_paths(FileTreeNode* node, const std::unordered_set<std::string>& paths) {
    if (node->is_directory && paths.count(node->full_path) > 0) {
        node->expanded = true;
        if (node->children.empty()) {
            load_children(*node);
        }
    }
    for (auto& child : node->children) {
        restore_expanded_paths(&child, paths);
    }
}

void FileTree::load_directory(const std::string& path) {
    std::filesystem::path fs_path = std::filesystem::absolute(path);
    fs_path = std::filesystem::canonical(fs_path);
    root_path = fs_path.string();
    root.name = fs_path.filename().string();
    root.full_path = root_path;
    root.is_directory = true;
    root.expanded = true;
    root.depth = 0;
    root.children.clear();
    load_children(root);
    rebuild_visible();
    refresh_git_status_async();

    current_fs_snapshot.clear();
    current_fs_snapshot.insert(root_path);
    collect_fs_snapshot(root_path, current_fs_snapshot);
    Uint32 now = SDL_GetTicks();
    last_fs_scan_time = now;
    last_git_scan_time = now;
}

void FileTree::load_children(FileTreeNode& node) {
    if (!node.is_directory) return;
    node.children.clear();

    std::vector<FileTreeNode> dirs;
    std::vector<FileTreeNode> files;

    try {
        for (const auto& entry : std::filesystem::directory_iterator(node.full_path)) {
            FileTreeNode child;
            child.name = entry.path().filename().string();

            if (!show_hidden_files && !child.name.empty() && child.name[0] == '.') {
                continue;
            }

            child.full_path = entry.path().string();
            child.is_directory = entry.is_directory();
            child.expanded = false;
            child.depth = node.depth + 1;

            if (child.is_directory) {
                dirs.push_back(child);
            } else {
                files.push_back(child);
            }
        }
    } catch (...) {}

    std::sort(dirs.begin(), dirs.end(), [](const FileTreeNode& a, const FileTreeNode& b) {
        return a.name < b.name;
    });
    std::sort(files.begin(), files.end(), [](const FileTreeNode& a, const FileTreeNode& b) {
        return a.name < b.name;
    });

    for (auto& d : dirs) node.children.push_back(std::move(d));
    for (auto& f : files) node.children.push_back(std::move(f));
}

void FileTree::rebuild_visible() {
    visible_nodes.clear();
    add_visible_recursive(&root);
}

void FileTree::add_visible_recursive(FileTreeNode* node) {
    visible_nodes.push_back(node);
    if (node->is_directory && node->expanded) {
        for (auto& child : node->children) {
            add_visible_recursive(&child);
        }
    }
}

void FileTree::toggle_expand() {
    auto& nodes = is_filtering() ? filtered_nodes : visible_nodes;
    if (selected_index < 0 || selected_index >= static_cast<int>(nodes.size())) return;
    FileTreeNode* node = nodes[selected_index];
    if (!node->is_directory || is_root_node(node)) return;

    node->expanded = !node->expanded;
    if (node->expanded && node->children.empty()) {
        load_children(*node);
    }
    rebuild_visible();
    if (is_filtering()) {
        apply_filter();
    }
}

void FileTree::move_up() {
    if (selected_index > 0) {
        selected_index--;
    }
}

void FileTree::move_down() {
    auto& nodes = is_filtering() ? filtered_nodes : visible_nodes;
    if (selected_index < static_cast<int>(nodes.size()) - 1) {
        selected_index++;
    }
}

FileTreeNode* FileTree::get_selected() {
    auto& nodes = is_filtering() ? filtered_nodes : visible_nodes;
    if (selected_index >= 0 && selected_index < static_cast<int>(nodes.size())) {
        return nodes[selected_index];
    }
    return nullptr;
}

bool FileTree::is_filtering() const {
    return !filter_query.empty();
}

void FileTree::set_filter(const std::string& query) {
    filter_query = query;
    apply_filter();
}

void FileTree::clear_filter() {
    filter_query.clear();
    filtered_nodes.clear();
    restore_expanded_state();
    rebuild_visible();
    selected_index = 0;
}

void FileTree::clear_filter_and_select(FileTreeNode* node) {
    filter_query.clear();
    filtered_nodes.clear();
    restore_expanded_state();
    if (node) {
        expand_path_to_node(node);
    }
    rebuild_visible();
    if (node) {
        for (int i = 0; i < static_cast<int>(visible_nodes.size()); i++) {
            if (visible_nodes[i] == node) {
                selected_index = i;
                break;
            }
        }
    } else {
        selected_index = 0;
    }
}

void FileTree::apply_filter() {
    filtered_nodes.clear();
    if (filter_query.empty()) return;

    std::string lower_query = filter_query;
    std::transform(lower_query.begin(), lower_query.end(), lower_query.begin(), ::tolower);

    collect_matching_nodes(&root, lower_query);
    selected_index = filtered_nodes.empty() ? -1 : 0;
}

void FileTree::collect_matching_nodes(FileTreeNode* node, const std::string& lower_query) {
    std::string lower_name = node->name;
    std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);

    if (lower_name.find(lower_query) != std::string::npos) {
        filtered_nodes.push_back(node);
    }

    for (auto& child : node->children) {
        collect_matching_nodes(&child, lower_query);
    }
}

void FileTree::save_expanded_state() {
    expanded_before_filter.clear();
    save_expanded_recursive(&root);
}

void FileTree::save_expanded_recursive(FileTreeNode* node) {
    if (node->is_directory && node->expanded) {
        expanded_before_filter.insert(node->full_path);
    }
    for (auto& child : node->children) {
        save_expanded_recursive(&child);
    }
}

void FileTree::restore_expanded_state() {
    restore_expanded_recursive(&root);
    expanded_before_filter.clear();
}

void FileTree::restore_expanded_recursive(FileTreeNode* node) {
    if (node->is_directory) {
        bool should_expand = expanded_before_filter.count(node->full_path) > 0;
        if (should_expand) {
            if (node->children.empty()) {
                load_children(*node);
            }
            node->expanded = true;
        } else {
            node->expanded = false;
        }
    }
    for (auto& child : node->children) {
        restore_expanded_recursive(&child);
    }
}

void FileTree::expand_path_to_node(FileTreeNode* target) {
    expand_path_recursive(&root, target->full_path);
}

bool FileTree::expand_path_recursive(FileTreeNode* node, const std::string& target_path) {
    if (node->full_path == target_path) {
        return true;
    }
    if (node->is_directory) {
        for (auto& child : node->children) {
            if (target_path.find(node->full_path) == 0) {
                if (expand_path_recursive(&child, target_path)) {
                    node->expanded = true;
                    return true;
                }
            }
        }
    }
    return false;
}

void FileTree::select_by_path(const std::string& path) {
    for (int i = 0; i < static_cast<int>(visible_nodes.size()); i++) {
        if (visible_nodes[i]->full_path == path) {
            selected_index = i;
            return;
        }
    }
    if (selected_index >= static_cast<int>(visible_nodes.size())) {
        selected_index = std::max(0, static_cast<int>(visible_nodes.size()) - 1);
    }
}

void FileTree::expand_all_for_filter() {
    save_expanded_state();
    expand_recursive(&root);
    rebuild_visible();
}

void FileTree::expand_and_select_path(const std::string& target_path) {
    expand_path_by_string(&root, target_path);
    rebuild_visible();
    for (int i = 0; i < static_cast<int>(visible_nodes.size()); i++) {
        if (visible_nodes[i]->full_path == target_path) {
            selected_index = i;
            break;
        }
    }
}

bool FileTree::expand_path_by_string(FileTreeNode* node, const std::string& target_path) {
    if (node->full_path == target_path) {
        return true;
    }
    if (node->is_directory && target_path.find(node->full_path + "/") == 0) {
        if (node->children.empty()) {
            load_children(*node);
        }
        node->expanded = true;
        for (auto& child : node->children) {
            if (expand_path_by_string(&child, target_path)) {
                return true;
            }
        }
    }
    return false;
}

void FileTree::expand_recursive(FileTreeNode* node) {
    if (node->is_directory) {
        if (node->children.empty()) {
            load_children(*node);
        }
        node->expanded = true;
        for (auto& child : node->children) {
            expand_recursive(&child);
        }
    }
}

void FileTree::ensure_visible(int visible_lines) {
    if (selected_index < scroll_offset) {
        scroll_offset = selected_index;
    }
    if (selected_index >= scroll_offset + visible_lines) {
        scroll_offset = selected_index - visible_lines + 1;
    }
    scroll_offset = std::max(0, scroll_offset);
}

bool FileTree::is_loaded() const {
    return !root_path.empty();
}

bool FileTree::handle_text_input(const char* text) {
    if (!is_loaded()) return false;

    if (filter_query.empty()) {
        expand_all_for_filter();
    }
    set_filter(filter_query + text);
    return true;
}

FileTreeInputResult FileTree::handle_key_event(const SDL_Event& event, int visible_lines, bool file_is_open) {
    FileTreeInputResult result;
    if (!is_loaded()) return result;

    result.consumed = true;
    bool ctrl = (event.key.keysym.mod & META_MOD) != 0;

    switch (event.key.keysym.sym) {
        case SDLK_ESCAPE:
            if (is_filtering()) {
                clear_filter_and_select(nullptr);
            } else if (file_is_open) {
                result.action = FileTreeAction::FocusEditor;
            } else {
                result.action = FileTreeAction::Exit;
            }
            break;
        case SDLK_BACKSPACE:
            if (is_filtering()) {
                std::string q = filter_query;
                if (!q.empty()) {
                    int prev = utf8_prev_char_start(q, static_cast<int>(q.size()));
                    q = q.substr(0, prev);
                    if (q.empty()) {
                        clear_filter_and_select(nullptr);
                    } else {
                        set_filter(q);
                    }
                }
            }
            break;
        case SDLK_UP:
        case SDLK_k:
            move_up();
            ensure_visible(visible_lines);
            break;
        case SDLK_DOWN:
        case SDLK_j:
            move_down();
            ensure_visible(visible_lines);
            break;
        case SDLK_RETURN: {
            FileTreeNode* selected = get_selected();
            if (selected) {
                if (selected->is_directory) {
                    if (is_filtering()) {
                        clear_filter_and_select(selected);
                    }
                    toggle_expand();
                } else {
                    if (is_filtering()) {
                        clear_filter_and_select(selected);
                    }
                    result.action = FileTreeAction::OpenFile;
                    result.path = selected->full_path;
                }
            }
            break;
        }
        case SDLK_LEFT:
        case SDLK_h: {
            if (!is_filtering()) {
                FileTreeNode* selected = get_selected();
                if (selected && selected->is_directory && selected->expanded) {
                    toggle_expand();
                }
            }
            break;
        }
        case SDLK_RIGHT:
        case SDLK_l: {
            if (!is_filtering()) {
                FileTreeNode* selected = get_selected();
                if (selected && selected->is_directory && !selected->expanded) {
                    toggle_expand();
                }
            }
            break;
        }
        case SDLK_n: {
            if (ctrl && !is_filtering()) {
                FileTreeNode* selected = get_selected();
                if (selected && !is_root_node(selected)) {
                    result.action = FileTreeAction::StartCreate;
                    result.path = selected->is_directory
                        ? selected->full_path
                        : std::filesystem::path(selected->full_path).parent_path().string();
                }
            }
            break;
        }
        case SDLK_DELETE: {
            if (!is_filtering()) {
                FileTreeNode* selected = get_selected();
                if (selected && !is_root_node(selected)) {
                    result.action = FileTreeAction::StartDelete;
                    result.path = selected->full_path;
                    result.name = selected->name;
                }
            }
            break;
        }
        default:
            result.consumed = false;
            break;
    }

    return result;
}

void FileTree::handle_scroll(int wheel_y, int visible_lines) {
    scroll_offset -= wheel_y * 3;
    scroll_offset = std::max(0, scroll_offset);
    int max_scroll = std::max(0, static_cast<int>(visible_nodes.size()) - visible_lines);
    scroll_offset = std::min(scroll_offset, max_scroll);
}

void FileTree::render(SDL_Renderer* renderer, TTF_Font* font, TextureCache& texture_cache,
                      int x, int y, int width, int height,
                      int line_height,
                      bool has_focus, bool cursor_visible,
                      const std::string& current_editor_path) {
    if (!is_loaded()) return;

    SDL_SetRenderDrawColor(renderer, Colors::GUTTER.r, Colors::GUTTER.g, Colors::GUTTER.b, 255);
    SDL_Rect tree_bg = {x, y, width, height};
    SDL_RenderFillRect(renderer, &tree_bg);

    SDL_SetRenderDrawColor(renderer, 50, 50, 55, 255);
    SDL_Rect tree_border = {x + width - 1, y, 1, height};
    SDL_RenderFillRect(renderer, &tree_border);

    render_toolbar(renderer, font, texture_cache, x, y, width);
    int toolbar_offset = TOOLBAR_HEIGHT;

    int filter_bar_height = 0;
    if (is_filtering()) {
        filter_bar_height = line_height + PADDING;
        SDL_SetRenderDrawColor(renderer, Colors::SEARCH_BG.r, Colors::SEARCH_BG.g, Colors::SEARCH_BG.b, 255);
        SDL_Rect filter_bg = {x, y + toolbar_offset, width, filter_bar_height};
        SDL_RenderFillRect(renderer, &filter_bg);

        std::string filter_text = " " + filter_query;
        texture_cache.render_cached_text(filter_text, Colors::TEXT, x + PADDING, y + toolbar_offset + PADDING / 2);

        if (cursor_visible && has_focus) {
            int filter_w = 0;
            TTF_SizeUTF8(font, filter_text.c_str(), &filter_w, nullptr);
            SDL_SetRenderDrawColor(renderer, Colors::CURSOR.r, Colors::CURSOR.g, Colors::CURSOR.b, 255);
            SDL_Rect filter_cursor = {x + PADDING + filter_w, y + toolbar_offset + PADDING / 2, 2, line_height};
            SDL_RenderFillRect(renderer, &filter_cursor);
        }
    }

    int content_offset = toolbar_offset + filter_bar_height;
    SDL_Rect tree_clip = {x, y + content_offset, width, height - content_offset};
    SDL_RenderSetClipRect(renderer, &tree_clip);

    auto& display_nodes = is_filtering() ? filtered_nodes : visible_nodes;
    int tree_y = y + PADDING + content_offset;

    for (int idx = scroll_offset;
         idx < static_cast<int>(display_nodes.size()) && tree_y < y + height;
         idx++) {
        FileTreeNode* node = display_nodes[idx];

        if (idx == selected_index && has_focus) {
            SDL_SetRenderDrawColor(renderer, Colors::ACTIVE_LINE.r, Colors::ACTIVE_LINE.g, Colors::ACTIVE_LINE.b, 255);
            SDL_Rect sel_rect = {x, tree_y, width, line_height};
            SDL_RenderFillRect(renderer, &sel_rect);
        } else if (idx == selected_index) {
            SDL_SetRenderDrawColor(renderer, 35, 35, 40, 255);
            SDL_Rect sel_rect = {x, tree_y, width, line_height};
            SDL_RenderFillRect(renderer, &sel_rect);
        }

        if (idx == context_menu_index) {
            SDL_SetRenderDrawColor(renderer, Colors::CURSOR.r, Colors::CURSOR.g, Colors::CURSOR.b, 255);
            SDL_Rect border_rect = {x + 1, tree_y, width - 3, line_height};
            SDL_RenderDrawRect(renderer, &border_rect);
        }

        int indent = is_filtering() ? 0 : node->depth * 16;
        std::string prefix;
        if (node->is_directory) {
            prefix = node->expanded ? "  " : "  ";
        } else {
            prefix = " ";
        }
        std::string display_name = prefix + node->name;

        SDL_Color node_color = Colors::TEXT;
        if (node->is_directory) {
            node_color = get_directory_git_color(node->full_path);
        } else {
            if (is_file_ignored(node->full_path)) {
                node_color = Colors::GIT_IGNORED;
            } else if (is_file_staged(node->full_path)) {
                node_color = Colors::GIT_STAGED;
            } else if (is_file_modified(node->full_path)) {
                node_color = Colors::GIT_MODIFIED;
            } else if (is_file_untracked(node->full_path)) {
                node_color = Colors::GIT_UNTRACKED;
            }
        }

        if (!current_editor_path.empty() && node->full_path == current_editor_path) {
            node_color = Colors::SYNTAX_KEYWORD;
        }

        texture_cache.render_cached_text(display_name, node_color, x + PADDING + indent, tree_y);
        tree_y += line_height;
    }

    SDL_RenderSetClipRect(renderer, nullptr);
}

void FileTree::handle_mouse_click(int /* x */, int y, int line_height) {
    if (!is_loaded()) return;

    int filter_bar_height = is_filtering() ? line_height + PADDING : 0;
    int content_y_start = TOOLBAR_HEIGHT + PADDING + filter_bar_height;

    if (y < content_y_start) return;

    int clicked_index = scroll_offset + (y - content_y_start) / line_height;

    auto& display_nodes = is_filtering() ? filtered_nodes : visible_nodes;
    if (clicked_index < 0 || clicked_index >= static_cast<int>(display_nodes.size())) return;

    selected_index = clicked_index;
    FileTreeNode* node = display_nodes[selected_index];

    if (node->is_directory && !is_root_node(node)) {
        node->expanded = !node->expanded;
        if (node->expanded && node->children.empty()) {
            load_children(*node);
        }
        rebuild_visible();
        if (is_filtering()) {
            apply_filter();
        }
    }
}

std::string FileTree::handle_mouse_double_click(int /* x */, int y, int line_height) {
    if (!is_loaded()) return "";

    int filter_bar_height = is_filtering() ? line_height + PADDING : 0;
    int content_y_start = TOOLBAR_HEIGHT + PADDING + filter_bar_height;

    if (y < content_y_start) return "";

    int clicked_index = scroll_offset + (y - content_y_start) / line_height;

    auto& display_nodes = is_filtering() ? filtered_nodes : visible_nodes;
    if (clicked_index < 0 || clicked_index >= static_cast<int>(display_nodes.size())) return "";

    selected_index = clicked_index;
    FileTreeNode* node = display_nodes[selected_index];

    if (!node->is_directory) {
        return node->full_path;
    }
    return "";
}

FileTreeNode* FileTree::get_node_at_position(int y, int line_height) {
    int idx = get_index_at_position(y, line_height);
    if (idx < 0) return nullptr;
    auto& display_nodes = is_filtering() ? filtered_nodes : visible_nodes;
    return display_nodes[idx];
}

int FileTree::get_index_at_position(int y, int line_height) {
    if (!is_loaded()) return -1;

    int filter_bar_height = is_filtering() ? line_height + PADDING : 0;
    int content_y_start = TOOLBAR_HEIGHT + PADDING + filter_bar_height;

    if (y < content_y_start) return -1;

    int clicked_index = scroll_offset + (y - content_y_start) / line_height;

    auto& display_nodes = is_filtering() ? filtered_nodes : visible_nodes;
    if (clicked_index < 0 || clicked_index >= static_cast<int>(display_nodes.size())) return -1;

    return clicked_index;
}

void FileTree::collapse_all() {
    std::function<void(FileTreeNode*)> collapse_recursive = [&](FileTreeNode* node) {
        if (node->is_directory && !is_root_node(node)) {
            node->expanded = false;
        }
        for (auto& child : node->children) {
            collapse_recursive(&child);
        }
    };
    collapse_recursive(&root);
    rebuild_visible();
    selected_index = 0;
    scroll_offset = 0;
}

void FileTree::scroll_to_path(const std::string& path, int visible_lines) {
    if (path.empty() || !is_loaded()) return;
    if (is_filtering()) {
        clear_filter_and_select(nullptr);
    }
    expand_and_select_path(path);
    ensure_visible(visible_lines);
}

void FileTree::toggle_hidden_files() {
    show_hidden_files = !show_hidden_files;
    std::unordered_set<std::string> expanded_paths;
    collect_expanded_paths(&root, expanded_paths);
    root.children.clear();
    load_children(root);
    restore_expanded_paths(&root, expanded_paths);
    rebuild_visible();
}

void FileTree::reveal_in_file_manager(const std::string& path) {
    std::string dir_path = path;
    if (!std::filesystem::is_directory(path)) {
        dir_path = std::filesystem::path(path).parent_path().string();
    }
    std::string cmd = "xdg-open \"" + dir_path + "\" 2>/dev/null &";
    std::ignore = system(cmd.c_str());
}

bool FileTree::has_git_changes_in_directory(const std::string& dir_path) const {
    auto check_prefix = [&dir_path](const std::unordered_set<std::string>& files) {
        for (const auto& file : files) {
            if (file.find(dir_path + "/") == 0) return true;
        }
        return false;
    };
    return check_prefix(git_staged_files) ||
           check_prefix(git_modified_files) ||
           check_prefix(git_untracked_files);
}

SDL_Color FileTree::get_directory_git_color(const std::string& dir_path) const {
    if (is_file_ignored(dir_path)) {
        return Colors::GIT_IGNORED;
    }
    if (is_file_untracked(dir_path)) {
        return Colors::GIT_UNTRACKED;
    }

    bool has_staged = false;
    bool has_modified = false;

    for (const auto& file : git_staged_files) {
        if (file.find(dir_path + "/") == 0) { has_staged = true; break; }
    }
    for (const auto& file : git_modified_files) {
        if (file.find(dir_path + "/") == 0) { has_modified = true; break; }
    }

    if (has_modified) return Colors::GIT_MODIFIED;
    if (has_staged) return Colors::GIT_STAGED;
    return Colors::SYNTAX_FUNCTION;
}

void FileTree::render_toolbar(SDL_Renderer* renderer, TTF_Font* font, TextureCache& texture_cache, int x, int y, int width) {
    toolbar_width_ = width;

    SDL_SetRenderDrawColor(renderer, 35, 35, 42, 255);
    SDL_Rect toolbar_bg = {x, y, width, TOOLBAR_HEIGHT};
    SDL_RenderFillRect(renderer, &toolbar_bg);

    SDL_SetRenderDrawColor(renderer, 50, 50, 55, 255);
    SDL_RenderDrawLine(renderer, x, y + TOOLBAR_HEIGHT - 1, x + width, y + TOOLBAR_HEIGHT - 1);

    int btn_y = y + (TOOLBAR_HEIGHT - TOOLBAR_BUTTON_SIZE) / 2;

    struct ToolbarButton {
        const char* icon;
        bool active;
    };

    ToolbarButton buttons[] = {
        {"", false},
        {show_hidden_files ? "" : "", show_hidden_files},
        {"󰡍", false}
    };

    for (int i = 0; i < 3; ++i) {
        int btn_x = x + width - PADDING - TOOLBAR_BUTTON_SIZE - i * (TOOLBAR_BUTTON_SIZE + TOOLBAR_BUTTON_GAP);

        bool is_hovered = (hovered_toolbar_button == i);

        if (is_hovered) {
            SDL_SetRenderDrawColor(renderer, 60, 60, 70, 255);
            SDL_Rect btn_bg = {btn_x - 2, btn_y - 2, TOOLBAR_BUTTON_SIZE + 4, TOOLBAR_BUTTON_SIZE + 4};
            SDL_RenderFillRect(renderer, &btn_bg);
        }

        SDL_Color btn_color = buttons[i].active ? Colors::SYNTAX_KEYWORD :
                              is_hovered ? Colors::TEXT : SDL_Color{140, 140, 150, 255};

        int text_w = 0, text_h = 0;
        TTF_SizeUTF8(font, buttons[i].icon, &text_w, &text_h);
        int text_x = btn_x + (TOOLBAR_BUTTON_SIZE - text_w) / 2;
        int text_y = btn_y + (TOOLBAR_BUTTON_SIZE - text_h) / 2;

        texture_cache.render_cached_text(buttons[i].icon, btn_color, text_x, text_y);
    }
}

FileTreeToolbarAction FileTree::handle_toolbar_click(int local_x, int local_y, int width) {
    if (local_y < 0 || local_y >= TOOLBAR_HEIGHT) return FileTreeToolbarAction::None;

    for (int i = 0; i < 3; ++i) {
        int btn_left = width - PADDING - TOOLBAR_BUTTON_SIZE - i * (TOOLBAR_BUTTON_SIZE + TOOLBAR_BUTTON_GAP);
        int btn_right = btn_left + TOOLBAR_BUTTON_SIZE;

        if (local_x >= btn_left && local_x < btn_right) {
            switch (i) {
                case 0: return FileTreeToolbarAction::NewFile;
                case 1: return FileTreeToolbarAction::ToggleHidden;
                case 2: return FileTreeToolbarAction::CollapseAll;
            }
        }
    }
    return FileTreeToolbarAction::None;
}

void FileTree::update_toolbar_hover(int local_x, int local_y, int width) {
    hovered_toolbar_button = -1;
    if (local_y < 0 || local_y >= TOOLBAR_HEIGHT) return;

    for (int i = 0; i < 3; ++i) {
        int btn_left = width - PADDING - TOOLBAR_BUTTON_SIZE - i * (TOOLBAR_BUTTON_SIZE + TOOLBAR_BUTTON_GAP);
        int btn_right = btn_left + TOOLBAR_BUTTON_SIZE;

        if (local_x >= btn_left && local_x < btn_right) {
            hovered_toolbar_button = i;
            return;
        }
    }
}
