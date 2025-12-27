#pragma once

#include <vector>
#include <cstdint>
#include <string>

class LineOffsetTree {
public:
    void build_from_lines(const std::vector<std::string>& lines) {
        actual_lines = lines.size();
        size = 1;
        while (size <= actual_lines) size *= 2;

        tree.assign(size + 1, 0);
        line_lengths.resize(actual_lines);

        for (size_t i = 0; i < actual_lines; ++i) {
            line_lengths[i] = static_cast<uint32_t>(lines[i].size() + 1);
        }

        for (size_t i = 1; i <= actual_lines; ++i) {
            tree[i] += line_lengths[i - 1];
            size_t parent = i + (i & -i);
            if (parent <= size) {
                tree[parent] += tree[i];
            }
        }
    }

    void update(int line_idx, int delta) {
        if (delta == 0) return;
        line_lengths[line_idx] += delta;
        size_t index = line_idx + 1;
        while (index <= size) {
            tree[index] += delta;
            index += index & (-index);
        }
    }

    uint32_t get_line_start_offset(int line_idx) const {
        uint32_t sum = 0;
        size_t index = static_cast<size_t>(line_idx);
        while (index > 0) {
            sum += tree[index];
            index -= index & (-index);
        }
        return sum;
    }

    uint32_t get_line_end_offset(int line_idx) const {
        return get_line_start_offset(line_idx + 1);
    }

    int find_line_by_offset(uint32_t byte_offset) const {
        size_t idx = 0;
        uint32_t current_sum = 0;

        for (size_t mask = size / 2; mask > 0; mask >>= 1) {
            size_t next_idx = idx + mask;
            if (next_idx <= actual_lines && current_sum + tree[next_idx] <= byte_offset) {
                idx = next_idx;
                current_sum += tree[idx];
            }
        }
        return static_cast<int>(idx);
    }

    void insert_line(int line_idx, uint32_t length) {
        line_lengths.insert(line_lengths.begin() + line_idx, length);
        actual_lines++;
        if (actual_lines > size) {
            size *= 2;
        }
        rebuild_tree();
    }

    void remove_line(int line_idx) {
        line_lengths.erase(line_lengths.begin() + line_idx);
        actual_lines--;
        rebuild_tree();
    }

    void set_line_length(int line_idx, uint32_t new_length) {
        int delta = static_cast<int>(new_length) - static_cast<int>(line_lengths[line_idx]);
        update(line_idx, delta);
    }

    uint32_t get_line_length(int line_idx) const {
        return line_lengths[line_idx];
    }

    size_t line_count() const { return actual_lines; }

    uint32_t total_bytes() const {
        return get_line_start_offset(static_cast<int>(actual_lines));
    }

    void clear() {
        tree.clear();
        line_lengths.clear();
        actual_lines = 0;
        size = 0;
    }

    bool empty() const { return actual_lines == 0; }

private:
    std::vector<uint32_t> tree;
    std::vector<uint32_t> line_lengths;
    size_t size = 0;
    size_t actual_lines = 0;

    void rebuild_tree() {
        size = 1;
        while (size <= actual_lines) size *= 2;
        tree.assign(size + 1, 0);
        for (size_t i = 1; i <= actual_lines; ++i) {
            tree[i] += line_lengths[i - 1];
            size_t parent = i + (i & -i);
            if (parent <= size) {
                tree[parent] += tree[i];
            }
        }
    }
};
