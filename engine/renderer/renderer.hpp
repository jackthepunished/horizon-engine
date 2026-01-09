#pragma once

/**
 * @file renderer.hpp
 * @brief High-level OpenGL renderer interface
 */

#include "engine/core/types.hpp"
#include "engine/platform/window.hpp"
#include "opengl/gl_context.hpp"

#include <glm/glm.hpp>

namespace hz {

/**
 * @brief OpenGL Renderer
 *
 * Manages the render loop with clear-screen rendering.
 */
class Renderer {
public:
    /**
     * @brief Create a renderer for the given window
     */
    explicit Renderer(Window& window);

    /**
     * @brief Destroy the renderer
     */
    ~Renderer();

    HZ_NON_COPYABLE(Renderer);
    HZ_NON_MOVABLE(Renderer);

    // ========================================================================
    // Frame Lifecycle
    // ========================================================================

    /**
     * @brief Begin a new frame
     */
    void begin_frame();

    /**
     * @brief End the current frame and swap buffers
     */
    void end_frame();

    // ========================================================================
    // Properties
    // ========================================================================

    /**
     * @brief Get the framebuffer size
     */
    [[nodiscard]] std::pair<u32, u32> framebuffer_size() const;

    /**
     * @brief Set the clear color
     */
    void set_clear_color(f32 r, f32 g, f32 b, f32 a = 1.0f);

    /**
     * @brief Set the clear color from vec4
     */
    void set_clear_color(const glm::vec4& color);

    /**
     * @brief Enable/disable depth testing
     */
    void set_depth_test(bool enabled);

    /**
     * @brief Enable/disable face culling
     */
    void set_face_culling(bool enabled);

    /**
     * @brief Set viewport
     */
    void set_viewport(i32 x, i32 y, i32 width, i32 height);

private:
    Window* m_window;
    glm::vec4 m_clear_color{0.1f, 0.1f, 0.15f, 1.0f};
};

} // namespace hz
