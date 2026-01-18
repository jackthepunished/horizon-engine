#include "debug_renderer.hpp"

#include "engine/core/log.hpp"

#include <fstream>
#include <sstream>

#include <glad/glad.h>

namespace hz {

namespace {
std::string read_shader_file(const std::string& path) {
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file) {
        HZ_ERROR("DebugRenderer: Could not open shader file: {}", path);
        return "";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}
} // namespace

DebugRenderer::~DebugRenderer() {
    if (m_line_vao != 0) {
        shutdown();
    }
}

void DebugRenderer::init() {
    // Load shader sources from files
    std::string vert_source = read_shader_file("assets/shaders/debug_line.vert");
    std::string frag_source = read_shader_file("assets/shaders/debug_line.frag");

    if (vert_source.empty() || frag_source.empty()) {
        HZ_ERROR("DebugRenderer: Failed to load shader files");
        return;
    }

    // Create shader from sources
    m_line_shader = std::make_unique<gl::Shader>(vert_source, frag_source);

    // Create VAO/VBO for lines
    glGenVertexArrays(1, &m_line_vao);
    glGenBuffers(1, &m_line_vbo);

    glBindVertexArray(m_line_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_line_vbo);

    // Allocate buffer (will be updated each frame)
    glBufferData(GL_ARRAY_BUFFER, MAX_LINE_VERTICES * sizeof(DebugVertex), nullptr,
                 GL_DYNAMIC_DRAW);

    // Position attribute
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(DebugVertex),
                          reinterpret_cast<void*>(offsetof(DebugVertex, position)));

    // Color attribute
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(DebugVertex),
                          reinterpret_cast<void*>(offsetof(DebugVertex, color)));

    glBindVertexArray(0);

    HZ_ENGINE_INFO("DebugRenderer initialized");
}

void DebugRenderer::shutdown() {
    if (m_line_vbo != 0) {
        glDeleteBuffers(1, &m_line_vbo);
        m_line_vbo = 0;
    }
    if (m_line_vao != 0) {
        glDeleteVertexArrays(1, &m_line_vao);
        m_line_vao = 0;
    }
    m_line_shader.reset();
    m_line_vertices.clear();

    HZ_ENGINE_INFO("DebugRenderer shutdown");
}

void DebugRenderer::draw_line(const glm::vec3& start, const glm::vec3& end,
                              const glm::vec3& color) {
    if (m_line_vertices.size() + 2 > MAX_LINE_VERTICES) {
        HZ_ENGINE_WARN("DebugRenderer: line vertex buffer full");
        return;
    }

    m_line_vertices.push_back({start, color});
    m_line_vertices.push_back({end, color});
}

void DebugRenderer::draw_point(const glm::vec3& pos, float size, const glm::vec3& color) {
    // Draw point as a small 3D cross
    draw_line(pos - glm::vec3(size, 0, 0), pos + glm::vec3(size, 0, 0), color);
    draw_line(pos - glm::vec3(0, size, 0), pos + glm::vec3(0, size, 0), color);
    draw_line(pos - glm::vec3(0, 0, size), pos + glm::vec3(0, 0, size), color);
}

void DebugRenderer::draw_box(const glm::vec3& min, const glm::vec3& max, const glm::vec3& color) {
    // Bottom face
    draw_line({min.x, min.y, min.z}, {max.x, min.y, min.z}, color);
    draw_line({max.x, min.y, min.z}, {max.x, min.y, max.z}, color);
    draw_line({max.x, min.y, max.z}, {min.x, min.y, max.z}, color);
    draw_line({min.x, min.y, max.z}, {min.x, min.y, min.z}, color);

    // Top face
    draw_line({min.x, max.y, min.z}, {max.x, max.y, min.z}, color);
    draw_line({max.x, max.y, min.z}, {max.x, max.y, max.z}, color);
    draw_line({max.x, max.y, max.z}, {min.x, max.y, max.z}, color);
    draw_line({min.x, max.y, max.z}, {min.x, max.y, min.z}, color);

    // Vertical edges
    draw_line({min.x, min.y, min.z}, {min.x, max.y, min.z}, color);
    draw_line({max.x, min.y, min.z}, {max.x, max.y, min.z}, color);
    draw_line({max.x, min.y, max.z}, {max.x, max.y, max.z}, color);
    draw_line({min.x, min.y, max.z}, {min.x, max.y, max.z}, color);
}

void DebugRenderer::draw_axes(const glm::vec3& pos, float size) {
    draw_line(pos, pos + glm::vec3(size, 0, 0), glm::vec3(1, 0, 0)); // X = Red
    draw_line(pos, pos + glm::vec3(0, size, 0), glm::vec3(0, 1, 0)); // Y = Green
    draw_line(pos, pos + glm::vec3(0, 0, size), glm::vec3(0, 0, 1)); // Z = Blue
}

void DebugRenderer::draw_skeleton(const Skeleton& skeleton,
                                  const std::vector<glm::mat4>& bone_transforms,
                                  const glm::mat4& model_matrix, const glm::vec3& bone_color,
                                  const glm::vec3& joint_color) {
    if (bone_transforms.empty()) {
        return;
    }

    const glm::mat4 global_inverse = skeleton.global_inverse_transform();
    const glm::mat4 global_transform = glm::inverse(global_inverse);

    // For each bone, calculate world position and draw to parent
    for (u32 i = 0; i < skeleton.bone_count(); ++i) {
        const Bone* bone = skeleton.get_bone(static_cast<i32>(i));
        if (!bone)
            continue;

        // Get bone's world position from the final transform
        // bone_transforms[i] = global_inverse * global_bone_transform * offset
        // To get world position: model * inverse(global_inverse) * bone_transforms[i] *
        // inverse(offset) * origin Simplified: we'll extract position from the final skinning
        // matrix

        // The bone transform already includes the offset, so we need to undo it to get the bone
        // position
        glm::mat4 offset_inv = glm::inverse(bone->offset_matrix);
        glm::mat4 world_bone = model_matrix * global_transform * bone_transforms[i] * offset_inv;

        glm::vec3 bone_pos = glm::vec3(world_bone[3]);

        // Draw joint point
        draw_point(bone_pos, 0.02f, joint_color);

        // Draw line to parent
        if (bone->parent_id >= 0) {
            const Bone* parent = skeleton.get_bone(bone->parent_id);
            if (parent) {
                glm::mat4 parent_offset_inv = glm::inverse(parent->offset_matrix);
                glm::mat4 world_parent = model_matrix * global_transform *
                                         bone_transforms[static_cast<size_t>(bone->parent_id)] *
                                         parent_offset_inv;
                glm::vec3 parent_pos = glm::vec3(world_parent[3]);

                draw_line(parent_pos, bone_pos, bone_color);
            }
        }
    }
}

void DebugRenderer::render(const glm::mat4& view_projection) {
    if (m_line_vertices.empty()) {
        return;
    }

    // Upload vertices
    glBindBuffer(GL_ARRAY_BUFFER, m_line_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    static_cast<GLsizeiptr>(m_line_vertices.size() * sizeof(DebugVertex)),
                    m_line_vertices.data());

    // Render
    m_line_shader->bind();
    m_line_shader->set_mat4("u_ViewProjection", view_projection);

    glBindVertexArray(m_line_vao);

    // Disable depth write but keep depth test for proper occlusion
    glDepthMask(GL_FALSE);

    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(m_line_vertices.size()));

    glDepthMask(GL_TRUE);

    glBindVertexArray(0);

    // Clear for next frame
    m_line_vertices.clear();
}

void DebugRenderer::clear() {
    m_line_vertices.clear();
}

} // namespace hz
