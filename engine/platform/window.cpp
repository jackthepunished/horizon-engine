#include "window.hpp"

#include "engine/core/log.hpp"

#include <stdexcept>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace hz {

Window::Window(const WindowConfig& config) {
    // Initialize GLFW once
    if (!s_glfw_initialized) {
        glfwSetErrorCallback(glfw_error_callback);

        if (!glfwInit()) {
            HZ_ENGINE_ERROR("Failed to initialize GLFW");
            throw std::runtime_error("Failed to initialize GLFW");
        }
        s_glfw_initialized = true;
        HZ_ENGINE_TRACE("GLFW initialized");
    }

    // Configure OpenGL context
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#ifdef HZ_DEBUG
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#endif

    glfwWindowHint(GLFW_RESIZABLE, config.resizable ? GLFW_TRUE : GLFW_FALSE);
    glfwWindowHint(GLFW_DECORATED, config.decorated ? GLFW_TRUE : GLFW_FALSE);

    // Create window
    GLFWmonitor* monitor = config.fullscreen ? glfwGetPrimaryMonitor() : nullptr;
    m_window = glfwCreateWindow(static_cast<int>(config.width), static_cast<int>(config.height),
                                config.title.c_str(), monitor, nullptr);

    if (!m_window) {
        HZ_ENGINE_ERROR("Failed to create GLFW window");
        throw std::runtime_error("Failed to create GLFW window");
    }

    // Make OpenGL context current
    glfwMakeContextCurrent(m_window);

    // VSync
    glfwSwapInterval(config.vsync ? 1 : 0);

    // Store this pointer for callbacks
    glfwSetWindowUserPointer(m_window, this);

    // Set callbacks
    glfwSetFramebufferSizeCallback(m_window, glfw_resize_callback);
    glfwSetWindowCloseCallback(m_window, glfw_close_callback);
    glfwSetKeyCallback(m_window, glfw_key_callback);
    glfwSetCursorPosCallback(m_window, glfw_cursor_pos_callback);
    glfwSetMouseButtonCallback(m_window, glfw_mouse_button_callback);
    glfwSetScrollCallback(m_window, glfw_scroll_callback);

    auto [w, h] = framebuffer_size();
    HZ_ENGINE_INFO("Window created: {}x{} ('{}')", w, h, config.title);
}

Window::~Window() {
    if (m_window) {
        glfwDestroyWindow(m_window);
        HZ_ENGINE_TRACE("Window destroyed");
    }

    // Note: We don't terminate GLFW here as other windows might exist
}

void Window::poll_events() {
    glfwPollEvents();
}

void Window::swap_buffers() {
    glfwSwapBuffers(m_window);
}

bool Window::should_close() const {
    return glfwWindowShouldClose(m_window) != 0;
}

void Window::close() {
    glfwSetWindowShouldClose(m_window, GLFW_TRUE);
}

std::pair<u32, u32> Window::size() const {
    int w, h;
    glfwGetWindowSize(m_window, &w, &h);
    return {static_cast<u32>(w), static_cast<u32>(h)};
}

std::pair<u32, u32> Window::framebuffer_size() const {
    int w, h;
    glfwGetFramebufferSize(m_window, &w, &h);
    return {static_cast<u32>(w), static_cast<u32>(h)};
}

bool Window::is_minimized() const {
    auto [w, h] = framebuffer_size();
    return w == 0 || h == 0;
}

void Window::set_cursor_captured(bool captured) {
    m_cursor_captured = captured;
    glfwSetInputMode(m_window, GLFW_CURSOR, captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);

    // Enable raw mouse motion if available
    if (captured && glfwRawMouseMotionSupported()) {
        glfwSetInputMode(m_window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    }
}

// ============================================================================
// GLFW Callbacks
// ============================================================================

void Window::glfw_error_callback(int error, const char* description) {
    HZ_ENGINE_ERROR("GLFW Error {}: {}", error, description);
}

void Window::glfw_resize_callback(GLFWwindow* window, int width, int height) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (self && self->m_resize_callback) {
        self->m_resize_callback({static_cast<u32>(width), static_cast<u32>(height)});
    }
}

void Window::glfw_close_callback(GLFWwindow* window) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (self && self->m_close_callback) {
        self->m_close_callback({});
    }
}

void Window::glfw_key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (self && self->m_key_callback) {
        self->m_key_callback({key, scancode, action, mods});
    }
}

void Window::glfw_cursor_pos_callback(GLFWwindow* window, double x, double y) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (self && self->m_mouse_move_callback) {
        self->m_mouse_move_callback({x, y});
    }
}

void Window::glfw_mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (self && self->m_mouse_button_callback) {
        self->m_mouse_button_callback({button, action, mods});
    }
}

void Window::glfw_scroll_callback(GLFWwindow* window, double x, double y) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (self && self->m_scroll_callback) {
        self->m_scroll_callback({x, y});
    }
}

} // namespace hz
