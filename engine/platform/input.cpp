#include "input.hpp"

#include "engine/core/log.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace hz {

InputManager::InputManager() {
    // Pre-register common FPS actions
    register_action("move_forward");
    register_action("move_backward");
    register_action("move_left");
    register_action("move_right");
    register_action("jump");
    register_action("crouch");
    register_action("sprint");
    register_action("primary_fire");
    register_action("secondary_fire");
    register_action("reload");
    register_action("interact");
    register_action("menu");

    HZ_ENGINE_DEBUG("InputManager initialized with {} pre-registered actions", m_actions.size());
}

void InputManager::attach(Window& window) {
    window.set_key_callback([this](const KeyEvent& e) { on_key_event(e); });
    window.set_mouse_button_callback(
        [this](const MouseButtonEvent& e) { on_mouse_button_event(e); });
    window.set_mouse_move_callback([this](const MouseMoveEvent& e) { on_mouse_move_event(e); });
    window.set_scroll_callback([this](const ScrollEvent& e) { on_scroll_event(e); });

    HZ_ENGINE_DEBUG("InputManager attached to window");
}

void InputManager::update() {
    // Update action states based on raw_pressed
    for (auto& action : m_actions) {
        switch (action.state) {
        case ActionState::Released:
            if (action.raw_pressed) {
                action.state = ActionState::JustPressed;
            }
            break;
        case ActionState::JustPressed:
            if (action.raw_pressed) {
                action.state = ActionState::Held;
            } else {
                action.state = ActionState::JustReleased;
            }
            break;
        case ActionState::Held:
            if (!action.raw_pressed) {
                action.state = ActionState::JustReleased;
            }
            break;
        case ActionState::JustReleased:
            if (action.raw_pressed) {
                action.state = ActionState::JustPressed;
            } else {
                action.state = ActionState::Released;
            }
            break;
        }
    }

    // Update mouse delta
    m_mouse.delta_x = m_mouse.x - m_last_mouse_x;
    m_mouse.delta_y = m_mouse.y - m_last_mouse_y;
    m_last_mouse_x = m_mouse.x;
    m_last_mouse_y = m_mouse.y;

    // Reset mouse delta on first update
    if (m_first_mouse_update) {
        m_mouse.delta_x = 0.0;
        m_mouse.delta_y = 0.0;
        m_first_mouse_update = false;
    }

    // Apply pending scroll
    m_mouse.scroll_x = m_pending_scroll_x;
    m_mouse.scroll_y = m_pending_scroll_y;
    m_pending_scroll_x = 0.0;
    m_pending_scroll_y = 0.0;
}

ActionId InputManager::register_action(std::string_view name) {
    auto id = static_cast<ActionId>(m_actions.size());
    m_actions.push_back({std::string(name), ActionState::Released, false});
    m_action_names[std::string(name)] = id;
    return id;
}

void InputManager::bind_key(ActionId action, i32 glfw_key) {
    m_key_bindings.emplace(glfw_key, action);
    HZ_ENGINE_TRACE("Bound key {} to action {}", glfw_key, m_actions[action].name);
}

void InputManager::bind_mouse_button(ActionId action, i32 glfw_button) {
    m_mouse_button_bindings.emplace(glfw_button, action);
    HZ_ENGINE_TRACE("Bound mouse button {} to action {}", glfw_button, m_actions[action].name);
}

std::optional<ActionId> InputManager::find_action(std::string_view name) const {
    auto it = m_action_names.find(std::string(name));
    if (it != m_action_names.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool InputManager::is_action_active(ActionId action) const {
    if (action >= m_actions.size())
        return false;
    auto state = m_actions[action].state;
    return state == ActionState::JustPressed || state == ActionState::Held;
}

bool InputManager::is_action_just_pressed(ActionId action) const {
    if (action >= m_actions.size())
        return false;
    return m_actions[action].state == ActionState::JustPressed;
}

bool InputManager::is_action_just_released(ActionId action) const {
    if (action >= m_actions.size())
        return false;
    return m_actions[action].state == ActionState::JustReleased;
}

ActionState InputManager::get_action_state(ActionId action) const {
    if (action >= m_actions.size())
        return ActionState::Released;
    return m_actions[action].state;
}

void InputManager::on_key_event(const KeyEvent& event) {
    bool pressed = (event.action == GLFW_PRESS || event.action == GLFW_REPEAT);

    auto range = m_key_bindings.equal_range(event.key);
    for (auto it = range.first; it != range.second; ++it) {
        m_actions[it->second].raw_pressed = pressed;
    }
}

void InputManager::on_mouse_button_event(const MouseButtonEvent& event) {
    bool pressed = (event.action == GLFW_PRESS);

    auto range = m_mouse_button_bindings.equal_range(event.button);
    for (auto it = range.first; it != range.second; ++it) {
        m_actions[it->second].raw_pressed = pressed;
    }
}

void InputManager::on_mouse_move_event(const MouseMoveEvent& event) {
    m_mouse.x = event.x;
    m_mouse.y = event.y;
}

void InputManager::on_scroll_event(const ScrollEvent& event) {
    m_pending_scroll_x += event.x_offset;
    m_pending_scroll_y += event.y_offset;
}

} // namespace hz
