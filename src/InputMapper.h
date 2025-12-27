#pragma once

#include <SDL2/SDL.h>
#include <string>
#include <string_view>
#include <unordered_map>
#include <functional>
#include <vector>
#include <optional>

struct KeyCombo {
    SDL_Keycode key = SDLK_UNKNOWN;
    uint16_t mod = 0;

    bool operator==(const KeyCombo&) const = default;

    static constexpr uint16_t normalize_mod(uint16_t raw_mod) {
        uint16_t result = 0;
        if (raw_mod & KMOD_CTRL) result |= KMOD_CTRL;
        if (raw_mod & KMOD_SHIFT) result |= KMOD_SHIFT;
        if (raw_mod & KMOD_ALT) result |= KMOD_ALT;
        if (raw_mod & KMOD_GUI) result |= KMOD_GUI;
        return result;
    }

    static KeyCombo from_event(const SDL_KeyboardEvent& event) {
        return {event.keysym.sym, normalize_mod(event.keysym.mod)};
    }
};

struct KeyComboHash {
    std::size_t operator()(const KeyCombo& k) const noexcept {
        return (std::hash<SDL_Keycode>{}(k.key) << 16) ^ std::hash<uint16_t>{}(k.mod);
    }
};

enum class InputContext {
    Global,
    Editor,
    FileTree,
    Terminal,
    CommandBar
};

class InputMapper {
public:
    void bind(KeyCombo combo, std::string_view action_id, InputContext context = InputContext::Global) {
        bindings_[context][combo] = std::string(action_id);
    }

    void unbind(KeyCombo combo, InputContext context = InputContext::Global) {
        if (auto it = bindings_.find(context); it != bindings_.end()) {
            it->second.erase(combo);
        }
    }

    std::optional<std::string_view> lookup(KeyCombo combo, InputContext context) const {
        if (auto ctx_it = bindings_.find(context); ctx_it != bindings_.end()) {
            if (auto it = ctx_it->second.find(combo); it != ctx_it->second.end()) {
                return it->second;
            }
        }
        if (context != InputContext::Global) {
            if (auto global_it = bindings_.find(InputContext::Global); global_it != bindings_.end()) {
                if (auto it = global_it->second.find(combo); it != global_it->second.end()) {
                    return it->second;
                }
            }
        }
        return std::nullopt;
    }

    std::optional<std::string_view> lookup(const SDL_KeyboardEvent& event, InputContext context) const {
        return lookup(KeyCombo::from_event(event), context);
    }

    void clear() { bindings_.clear(); }

    void clear_context(InputContext context) {
        bindings_.erase(context);
    }

    std::vector<std::pair<KeyCombo, std::string>> get_bindings(InputContext context) const {
        std::vector<std::pair<KeyCombo, std::string>> result;
        if (auto it = bindings_.find(context); it != bindings_.end()) {
            for (const auto& [combo, action] : it->second) {
                result.emplace_back(combo, action);
            }
        }
        return result;
    }

private:
    std::unordered_map<InputContext, std::unordered_map<KeyCombo, std::string, KeyComboHash>> bindings_;
};

namespace KeyMod {
    constexpr uint16_t None = 0;
    constexpr uint16_t Ctrl = KMOD_CTRL;
    constexpr uint16_t Shift = KMOD_SHIFT;
    constexpr uint16_t Alt = KMOD_ALT;
    constexpr uint16_t Meta = KMOD_GUI;
    constexpr uint16_t CtrlShift = KMOD_CTRL | KMOD_SHIFT;
    constexpr uint16_t CtrlAlt = KMOD_CTRL | KMOD_ALT;
    constexpr uint16_t AltShift = KMOD_ALT | KMOD_SHIFT;
}
