#pragma once

/**
 * @file window.hpp
 * @brief Window management abstraction
 *
 * Provides a platform-independent window interface implemented with GLFW.
 */

#include "engine/core/types.hpp"

#include <functional>
#include <string>

// Forward declare GLFW
struct GLFWwindow;

namespace hz {

// ============================================================================
// Window Configuration
// ============================================================================

struct WindowConfig {
    std::string title{"Horizon Engine"};
    u32 width{1280};
    u32 height{720};
    bool resizable{true};
    bool vsync{true};
    bool fullscreen{false};
    bool decorated{true};
};

// ============================================================================
// Window Events
// ============================================================================

struct WindowResizeEvent {
    u32 width;
    u32 height;
};

struct WindowCloseEvent {};

struct KeyEvent {
    i32 key;
    i32 scancode;
    i32 action;
    i32 mods;
};

struct MouseMoveEvent {
    f64 x;
    f64 y;
};

struct MouseButtonEvent {
    i32 button;
    i32 action;
    i32 mods;
};

struct ScrollEvent {
    f64 x_offset;
    f64 y_offset;
};

// ============================================================================
// Window Class
// ============================================================================

/**
 * @brief RAII window wrapper using GLFW
 */
class Window {
public:
    using ResizeCallback = std::function<void(const WindowResizeEvent&)>;
    using CloseCallback = std::function<void(const WindowCloseEvent&)>;
    using KeyCallback = std::function<void(const KeyEvent&)>;
    using MouseMoveCallback = std::function<void(const MouseMoveEvent&)>;
    using MouseButtonCallback = std::function<void(const MouseButtonEvent&)>;
    using ScrollCallback = std::function<void(const ScrollEvent&)>;

    /**
     * @brief Create a window with the given configuration
     */
    explicit Window(const WindowConfig& config = {});

    /**
     * @brief Destroy the window
     */
    ~Window() noexcept;

    HZ_NON_COPYABLE(Window);
    HZ_DEFAULT_MOVABLE(Window);

    // ========================================================================
    // Window Operations
    // ========================================================================

    /**
     * @brief Poll for window events
     */
    void poll_events();

    /**
     * @brief Swap the front and back buffers
     */
    void swap_buffers();

    /**
     * @brief Check if the window should close
     */
    [[nodiscard]] bool should_close() const;

    /**
     * @brief Request the window to close
     */
    void close();

    /**
     * @brief Get the window size
     */
    [[nodiscard]] std::pair<u32, u32> size() const;

    /**
     * @brief Get the framebuffer size (may differ from window size on HiDPI)
     */
    [[nodiscard]] std::pair<u32, u32> framebuffer_size() const;

    /**
     * @brief Check if the window is minimized
     */
    [[nodiscard]] bool is_minimized() const;

    /**
     * @brief Get the underlying GLFW window handle
     */
    [[nodiscard]] GLFWwindow* native_handle() const { return m_window; }

    // ========================================================================
    // Input State
    // ========================================================================

    /**
     * @brief Enable raw mouse input (for FPS camera)
     */
    void set_cursor_captured(bool captured);

    /**
     * @brief Check if cursor is captured
     */
    [[nodiscard]] bool is_cursor_captured() const { return m_cursor_captured; }

    // ========================================================================
    // Event Callbacks
    // ========================================================================

    void set_resize_callback(ResizeCallback cb) { m_resize_callback = std::move(cb); }
    void set_close_callback(CloseCallback cb) { m_close_callback = std::move(cb); }
    void set_key_callback(KeyCallback cb) { m_key_callback = std::move(cb); }
    void set_mouse_move_callback(MouseMoveCallback cb) { m_mouse_move_callback = std::move(cb); }
    void set_mouse_button_callback(MouseButtonCallback cb) {
        m_mouse_button_callback = std::move(cb);
    }
    void set_scroll_callback(ScrollCallback cb) { m_scroll_callback = std::move(cb); }

private:
    static void glfw_error_callback(int error, const char* description);
    static void glfw_resize_callback(GLFWwindow* window, int width, int height);
    static void glfw_close_callback(GLFWwindow* window);
    static void glfw_key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void glfw_cursor_pos_callback(GLFWwindow* window, double x, double y);
    static void glfw_mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
    static void glfw_scroll_callback(GLFWwindow* window, double x, double y);

    GLFWwindow* m_window{nullptr};
    bool m_cursor_captured{false};

    ResizeCallback m_resize_callback;
    CloseCallback m_close_callback;
    KeyCallback m_key_callback;
    MouseMoveCallback m_mouse_move_callback;
    MouseButtonCallback m_mouse_button_callback;
    ScrollCallback m_scroll_callback;

    static inline bool s_glfw_initialized{false};
};

} // namespace hz
