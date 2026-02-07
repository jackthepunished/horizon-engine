#include "debug_renderer.hpp"

namespace hz {

DebugRenderer::~DebugRenderer() = default;

void DebugRenderer::init() {
    // Stub
}

void DebugRenderer::shutdown() {
    // Stub
}

void DebugRenderer::draw_line(const glm::vec3& start, const glm::vec3& end,
                              const glm::vec3& color) {
    // Stub
}

void DebugRenderer::draw_point(const glm::vec3& pos, float size, const glm::vec3& color) {
    // Stub
}

void DebugRenderer::draw_box(const glm::vec3& min, const glm::vec3& max, const glm::vec3& color) {
    // Stub
}

void DebugRenderer::draw_axes(const glm::vec3& pos, float size) {
    // Stub
}

void DebugRenderer::draw_skeleton(const Skeleton& skeleton,
                                  const std::vector<glm::mat4>& bone_transforms,
                                  const glm::mat4& model_matrix, const glm::vec3& bone_color,
                                  const glm::vec3& joint_color) {
    // Stub
}

void DebugRenderer::render(const glm::mat4& view_projection) {
    // Stub
}

void DebugRenderer::clear() {
    m_line_vertices.clear();
}

} // namespace hz
