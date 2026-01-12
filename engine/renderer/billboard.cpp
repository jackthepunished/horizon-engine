#include "billboard.hpp"

#include "engine/core/log.hpp"
#include "opengl/gl_context.hpp"

namespace hz {

Billboard::Billboard(const BillboardConfig& config) : m_config(config) {
    init_quad();
    m_instances.reserve(config.max_instances);
    HZ_ENGINE_INFO("Billboard system initialized: max_instances={}", config.max_instances);
}

Billboard::~Billboard() noexcept {
    if (m_vao) {
        glDeleteVertexArrays(1, &m_vao);
        glDeleteBuffers(1, &m_quad_vbo);
        glDeleteBuffers(1, &m_instance_vbo);
    }
}

Billboard::Billboard(Billboard&& other) noexcept {
    *this = std::move(other);
}

Billboard& Billboard::operator=(Billboard&& other) noexcept {
    if (this == &other)
        return *this;

    if (m_vao) {
        glDeleteVertexArrays(1, &m_vao);
        glDeleteBuffers(1, &m_quad_vbo);
        glDeleteBuffers(1, &m_instance_vbo);
    }

    m_config = other.m_config;
    m_instances = std::move(other.m_instances);
    m_vao = other.m_vao;
    m_quad_vbo = other.m_quad_vbo;
    m_instance_vbo = other.m_instance_vbo;
    m_dirty = other.m_dirty;

    other.m_vao = 0;
    other.m_quad_vbo = 0;
    other.m_instance_vbo = 0;

    return *this;
}

void Billboard::init_quad() {
    // Simple quad centered at origin, facing +Z
    // Position (x, y, z), TexCoord (u, v)
    float quad_vertices[] = {
        // Position          // TexCoord
        -0.5f,  0.0f, 0.0f,  0.0f, 0.0f,  // Bottom-left (pivot at bottom)
         0.5f,  0.0f, 0.0f,  1.0f, 0.0f,  // Bottom-right
         0.5f,  1.0f, 0.0f,  1.0f, 1.0f,  // Top-right
        -0.5f,  0.0f, 0.0f,  0.0f, 0.0f,  // Bottom-left
         0.5f,  1.0f, 0.0f,  1.0f, 1.0f,  // Top-right
        -0.5f,  1.0f, 0.0f,  0.0f, 1.0f,  // Top-left
    };

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_quad_vbo);
    glGenBuffers(1, &m_instance_vbo);

    glBindVertexArray(m_vao);

    // Quad VBO
    glBindBuffer(GL_ARRAY_BUFFER, m_quad_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), quad_vertices, GL_STATIC_DRAW);

    // Position attribute (location 0)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);

    // TexCoord attribute (location 1)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));

    // Instance VBO
    glBindBuffer(GL_ARRAY_BUFFER, m_instance_vbo);
    glBufferData(GL_ARRAY_BUFFER, m_config.max_instances * sizeof(BillboardInstance), 
                 nullptr, GL_DYNAMIC_DRAW);

    // Instance position (location 3) - vec3
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(BillboardInstance), 
                         (void*)offsetof(BillboardInstance, position));
    glVertexAttribDivisor(3, 1);

    // Instance size (location 4) - vec2
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 2, GL_FLOAT, GL_FALSE, sizeof(BillboardInstance), 
                         (void*)offsetof(BillboardInstance, size));
    glVertexAttribDivisor(4, 1);

    // Instance color (location 5) - vec4
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(BillboardInstance), 
                         (void*)offsetof(BillboardInstance, color));
    glVertexAttribDivisor(5, 1);

    glBindVertexArray(0);
}

void Billboard::set_instances(const std::vector<BillboardInstance>& instances) {
    m_instances = instances;
    if (m_instances.size() > m_config.max_instances) {
        m_instances.resize(m_config.max_instances);
        HZ_ENGINE_WARN("Billboard instances capped at {}", m_config.max_instances);
    }
    m_dirty = true;
}

void Billboard::add_instance(const BillboardInstance& instance) {
    if (m_instances.size() < m_config.max_instances) {
        m_instances.push_back(instance);
        m_dirty = true;
    }
}

void Billboard::clear() {
    m_instances.clear();
    m_dirty = true;
}

void Billboard::upload() {
    if (!m_dirty || m_instances.empty())
        return;

    glBindBuffer(GL_ARRAY_BUFFER, m_instance_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, 
                    m_instances.size() * sizeof(BillboardInstance), 
                    m_instances.data());

    m_dirty = false;
}

void Billboard::draw() const {
    if (m_instances.empty())
        return;

    glBindVertexArray(m_vao);
    glDrawArraysInstanced(GL_TRIANGLES, 0, 6, static_cast<GLsizei>(m_instances.size()));
    glBindVertexArray(0);
}

} // namespace hz
