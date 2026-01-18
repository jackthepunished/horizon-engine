#pragma once

/**
 * @file debug_renderer.hpp
 * @brief Immediate-mode debug renderer for lines, points, and skeleton visualization
 */

#include "engine/animation/skeleton.hpp"
#include "engine/core/types.hpp"
#include "engine/renderer/opengl/shader.hpp"

#include <memory>
#include <vector>

#include <glm/glm.hpp>

namespace hz {

/**
 * @brief Vertex for debug line rendering
 */
struct DebugVertex {
    glm::vec3 position;
    glm::vec3 color;
};

/**
 * @brief Immediate-mode debug renderer
 *
 * Batches debug primitives (lines, points) and renders them in a single draw call.
 * Useful for visualizing skeletons, collision shapes, paths, etc.
 */
class DebugRenderer {
public:
    DebugRenderer() = default;
    ~DebugRenderer();

    HZ_NON_COPYABLE(DebugRenderer);
    HZ_NON_MOVABLE(DebugRenderer);

    /**
     * @brief Initialize GPU resources
     */
    void init();

    /**
     * @brief Cleanup GPU resources
     */
    void shutdown();

    // ========================================================================
    // Primitive Drawing (batched)
    // ========================================================================

    /**
     * @brief Draw a line segment
     */
    void draw_line(const glm::vec3& start, const glm::vec3& end,
                   const glm::vec3& color = glm::vec3(1.0f));

    /**
     * @brief Draw a point (rendered as small cross)
     */
    void draw_point(const glm::vec3& pos, float size = 0.05f,
                    const glm::vec3& color = glm::vec3(1.0f, 1.0f, 0.0f));

    /**
     * @brief Draw a wireframe box
     */
    void draw_box(const glm::vec3& min, const glm::vec3& max,
                  const glm::vec3& color = glm::vec3(1.0f));

    /**
     * @brief Draw coordinate axes at a position
     */
    void draw_axes(const glm::vec3& pos, float size = 1.0f);

    // ========================================================================
    // Skeleton Visualization
    // ========================================================================

    /**
     * @brief Draw skeleton bones as lines and joints as points
     *
     * @param skeleton The skeleton structure
     * @param bone_transforms Final bone transforms (from AnimatorComponent)
     * @param model_matrix The entity's world transform
     * @param bone_color Color for bone lines
     * @param joint_color Color for joint points
     */
    void draw_skeleton(const Skeleton& skeleton, const std::vector<glm::mat4>& bone_transforms,
                       const glm::mat4& model_matrix,
                       const glm::vec3& bone_color = glm::vec3(0.0f, 1.0f, 0.0f),
                       const glm::vec3& joint_color = glm::vec3(1.0f, 1.0f, 0.0f));

    // ========================================================================
    // Rendering
    // ========================================================================

    /**
     * @brief Render all batched primitives and clear the buffers
     *
     * @param view_projection Combined view-projection matrix
     */
    void render(const glm::mat4& view_projection);

    /**
     * @brief Clear all batched primitives without rendering
     */
    void clear();

    /**
     * @brief Check if there are pending primitives to render
     */
    [[nodiscard]] bool has_pending() const { return !m_line_vertices.empty(); }

private:
    std::unique_ptr<gl::Shader> m_line_shader;

    // GPU resources
    u32 m_line_vao{0};
    u32 m_line_vbo{0};

    // Batched vertices
    std::vector<DebugVertex> m_line_vertices;

    // Configuration
    static constexpr size_t MAX_LINE_VERTICES = 65536;
};

} // namespace hz
