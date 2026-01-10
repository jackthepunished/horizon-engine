#pragma once

/**
 * @file imgui_layer.hpp
 * @brief Dear ImGui integration layer for GLFW/OpenGL
 */

#include "engine/core/types.hpp"

// Forward declarations
struct GLFWwindow;

namespace hz {

class Window;

/**
 * @brief ImGui integration layer
 */
class ImGuiLayer {
public:
    ImGuiLayer() = default;
    ~ImGuiLayer();

    HZ_NON_COPYABLE(ImGuiLayer);
    HZ_NON_MOVABLE(ImGuiLayer);

    /**
     * @brief Initialize ImGui with GLFW window
     */
    void init(Window& window);

    /**
     * @brief Shutdown ImGui
     */
    void shutdown();

    /**
     * @brief Begin new ImGui frame
     */
    void begin_frame();

    /**
     * @brief End ImGui frame and render
     */
    void end_frame();

    /**
     * @brief Check if initialized
     */
    [[nodiscard]] bool is_initialized() const { return m_initialized; }

private:
    GLFWwindow* m_window{nullptr};
    bool m_initialized{false};
};

} // namespace hz
