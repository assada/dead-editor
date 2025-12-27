#pragma once

#include <unordered_map>
#include <list>
#include <cstddef>
#include <functional>
#ifdef __linux__
#include <malloc.h>
#endif

template <typename Key, typename Value, typename Hash = std::hash<Key>>
class LRUCache {
public:
    explicit LRUCache(size_t max_size) : max_size_(max_size) {}

    ~LRUCache() {
        clear_and_trim();
    }

    LRUCache(const LRUCache&) = delete;
    LRUCache& operator=(const LRUCache&) = delete;
    LRUCache(LRUCache&&) = default;
    LRUCache& operator=(LRUCache&&) = default;

    Value* get(const Key& key) {
        auto it = cache_.find(key);
        if (it == cache_.end()) return nullptr;
        touch(key);
        return &it->second;
    }

    Value& get_or_create(const Key& key) {
        touch(key);
        evict_if_needed();
        return cache_[key];
    }

    template <typename Factory>
    Value& get_or_create(const Key& key, Factory&& factory) {
        auto it = cache_.find(key);
        if (it != cache_.end()) {
            touch(key);
            return it->second;
        }
        touch(key);
        evict_if_needed();
        return cache_.emplace(key, factory()).first->second;
    }

    void invalidate(const Key& key) {
        cache_.erase(key);
        auto lru_it = lru_map_.find(key);
        if (lru_it != lru_map_.end()) {
            lru_order_.erase(lru_it->second);
            lru_map_.erase(lru_it);
        }
    }

    void clear() {
        {
            std::unordered_map<Key, Value, Hash> empty_cache;
            cache_.swap(empty_cache);
        }
        lru_order_.clear();
        {
            std::unordered_map<Key, typename std::list<Key>::iterator, Hash> empty_lru;
            lru_map_.swap(empty_lru);
        }
    }

    void clear_and_trim() {
        clear();
#ifdef __linux__
        malloc_trim(0);
#endif
    }

    size_t size() const { return cache_.size(); }
    bool empty() const { return cache_.empty(); }

    template <typename Func>
    void for_each(Func&& func) {
        for (auto& [key, value] : cache_) {
            func(key, value);
        }
    }

private:
    void touch(const Key& key) {
        auto it = lru_map_.find(key);
        if (it != lru_map_.end()) {
            lru_order_.erase(it->second);
        }
        lru_order_.push_front(key);
        lru_map_[key] = lru_order_.begin();
    }

    void evict_if_needed() {
        while (cache_.size() > max_size_ && !lru_order_.empty()) {
            Key oldest = lru_order_.back();
            lru_order_.pop_back();
            lru_map_.erase(oldest);
            cache_.erase(oldest);
        }
    }

    size_t max_size_;
    std::unordered_map<Key, Value, Hash> cache_;
    std::list<Key> lru_order_;
    std::unordered_map<Key, typename std::list<Key>::iterator, Hash> lru_map_;
};
