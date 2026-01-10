#pragma once

/**
 * @file input.hpp
 * @brief Action-based input system
 *
 * Maps physical inputs (keys, mouse buttons) to abstract actions.
 * Supports FPS-style raw mouse input for camera control.
 */

#include "engine/core/types.hpp"
#include "window.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace hz {

// ============================================================================
// Transparent String Hash (for heterogeneous lookup)
// ============================================================================

struct StringHash {
    using is_transparent = void;
    [[nodiscard]] std::size_t operator()(std::string_view sv) const noexcept {
        return std::hash<std::string_view>{}(sv);
    }
    [[nodiscard]] std::size_t operator()(const std::string& s) const noexcept {
        return std::hash<std::string_view>{}(s);
    }
    [[nodiscard]] std::size_t operator()(const char* s) const noexcept {
        return std::hash<std::string_view>{}(s);
    }
};

// ============================================================================
// Input Types
// ============================================================================

/**
 * @brief Abstract action identifier
 */
using ActionId = u32;

/**
 * @brief Action state
 */
enum class ActionState : u8 {
    Released,    // Not active
    JustPressed, // Became active this frame
    Held,        // Active for multiple frames
    JustReleased // Became inactive this frame
};

/**
 * @brief Mouse state
 */
struct MouseState {
    f64 x{0.0};
    f64 y{0.0};
    f64 delta_x{0.0};
    f64 delta_y{0.0};
    f64 scroll_x{0.0};
    f64 scroll_y{0.0};
};

// ============================================================================
// Input Binding
// ============================================================================

/**
 * @brief Binding from physical input to action
 */
struct InputBinding {
    enum class Type : u8 { Key, MouseButton };

    Type type{Type::Key};
    i32 code{0}; // GLFW key/button code
};

// ============================================================================
// Input Manager
// ============================================================================

/**
 * @brief Manages input state and action mapping
 */
class InputManager {
public:
    InputManager();

    /**
     * @brief Connect to a window for input events
     */
    void attach(Window& window);

    /**
     * @brief Update input state (call once per frame, before game logic)
     */
    void update();

    // ========================================================================
    // Action Mapping
    // ========================================================================

    /**
     * @brief Register a new action
     * @return The action ID
     */
    ActionId register_action(std::string_view name);

    /**
     * @brief Bind a key to an action
     */
    void bind_key(ActionId action, i32 glfw_key);

    /**
     * @brief Bind a mouse button to an action
     */
    void bind_mouse_button(ActionId action, i32 glfw_button);

    /**
     * @brief Get action ID by name
     */
    [[nodiscard]] std::optional<ActionId> find_action(std::string_view name) const;

    // ========================================================================
    // Action State
    // ========================================================================

    /**
     * @brief Check if an action is currently active (held or just pressed)
     */
    [[nodiscard]] bool is_action_active(ActionId action) const;

    /**
     * @brief Check if an action was just pressed this frame
     */
    [[nodiscard]] bool is_action_just_pressed(ActionId action) const;

    /**
     * @brief Check if an action was just released this frame
     */
    [[nodiscard]] bool is_action_just_released(ActionId action) const;

    /**
     * @brief Get the current state of an action
     */
    [[nodiscard]] ActionState get_action_state(ActionId action) const;

    // ========================================================================
    // Mouse State
    // ========================================================================

    /**
     * @brief Get the current mouse state
     */
    [[nodiscard]] const MouseState& mouse() const { return m_mouse; }

    /**
     * @brief Get mouse delta (movement since last frame)
     */
    [[nodiscard]] std::pair<f64, f64> mouse_delta() const {
        return {m_mouse.delta_x, m_mouse.delta_y};
    }

    // ========================================================================
    // Common Actions (pre-registered)
    // ========================================================================

    static constexpr ActionId ACTION_MOVE_FORWARD = 0;
    static constexpr ActionId ACTION_MOVE_BACKWARD = 1;
    static constexpr ActionId ACTION_MOVE_LEFT = 2;
    static constexpr ActionId ACTION_MOVE_RIGHT = 3;
    static constexpr ActionId ACTION_JUMP = 4;
    static constexpr ActionId ACTION_CROUCH = 5;
    static constexpr ActionId ACTION_SPRINT = 6;
    static constexpr ActionId ACTION_PRIMARY_FIRE = 7;
    static constexpr ActionId ACTION_SECONDARY_FIRE = 8;
    static constexpr ActionId ACTION_RELOAD = 9;
    static constexpr ActionId ACTION_INTERACT = 10;
    static constexpr ActionId ACTION_MENU = 11;

private:
    void on_key_event(const KeyEvent& event);
    void on_mouse_button_event(const MouseButtonEvent& event);
    void on_mouse_move_event(const MouseMoveEvent& event);
    void on_scroll_event(const ScrollEvent& event);

    struct ActionData {
        std::string name;
        ActionState state{ActionState::Released};
        bool raw_pressed{false}; // Current physical state
    };

    std::vector<ActionData> m_actions;
    std::unordered_map<std::string, ActionId, StringHash, std::equal_to<>> m_action_names;
    std::unordered_multimap<i32, ActionId> m_key_bindings;
    std::unordered_multimap<i32, ActionId> m_mouse_button_bindings;

    MouseState m_mouse;
    f64 m_last_mouse_x{0.0};
    f64 m_last_mouse_y{0.0};
    bool m_first_mouse_update{true};

    f64 m_pending_scroll_x{0.0};
    f64 m_pending_scroll_y{0.0};
};

} // namespace hz
