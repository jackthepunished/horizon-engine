#include "water.hpp"

#include "engine/core/log.hpp"
#include "opengl/gl_context.hpp"

#include <vector>

namespace hz {

Water::~Water() noexcept {
    if (m_vao) {
        glDeleteVertexArrays(1, &m_vao);
        glDeleteBuffers(1, &m_vbo);
        glDeleteBuffers(1, &m_ebo);
    }
}

Water::Water(Water&& other) noexcept {
    *this = std::move(other);
}

Water& Water::operator=(Water&& other) noexcept {
    if (this == &other)
        return *this;

    // Release existing resources
    if (m_vao) {
        glDeleteVertexArrays(1, &m_vao);
        glDeleteBuffers(1, &m_vbo);
        glDeleteBuffers(1, &m_ebo);
    }

    m_config = other.m_config;
    m_vao = other.m_vao;
    m_vbo = other.m_vbo;
    m_ebo = other.m_ebo;
    m_index_count = other.m_index_count;

    other.m_vao = 0;
    other.m_vbo = 0;
    other.m_ebo = 0;
    other.m_index_count = 0;

    return *this;
}

void Water::init(const WaterConfig& config) {
    m_config = config;
    create_mesh();
    HZ_ENGINE_INFO("Water plane initialized: size={}, height={}", config.size, config.height);
}

void Water::create_mesh() {
    // Clean up existing buffers
    if (m_vao) {
        glDeleteVertexArrays(1, &m_vao);
        glDeleteBuffers(1, &m_vbo);
        glDeleteBuffers(1, &m_ebo);
    }

    float half_size = m_config.size * 0.5f;

    // Water plane vertices (position + texcoord)
    // Higher subdivision for better wave detail
    const int subdivisions = 32;
    const float step = m_config.size / static_cast<float>(subdivisions);
    
    std::vector<float> vertices;
    vertices.reserve((subdivisions + 1) * (subdivisions + 1) * 5);
    
    for (int z = 0; z <= subdivisions; ++z) {
        for (int x = 0; x <= subdivisions; ++x) {
            float px = -half_size + x * step;
            float pz = -half_size + z * step;
            float u = static_cast<float>(x) / static_cast<float>(subdivisions);
            float v = static_cast<float>(z) / static_cast<float>(subdivisions);
            
            vertices.push_back(px);                  // x
            vertices.push_back(m_config.height);     // y
            vertices.push_back(pz);                  // z
            vertices.push_back(u);                   // u
            vertices.push_back(v);                   // v
        }
    }

    // Generate indices
    std::vector<u32> indices;
    indices.reserve(subdivisions * subdivisions * 6);
    
    for (int z = 0; z < subdivisions; ++z) {
        for (int x = 0; x < subdivisions; ++x) {
            int top_left = z * (subdivisions + 1) + x;
            int top_right = top_left + 1;
            int bottom_left = (z + 1) * (subdivisions + 1) + x;
            int bottom_right = bottom_left + 1;
            
            // First triangle
            indices.push_back(static_cast<u32>(top_left));
            indices.push_back(static_cast<u32>(bottom_left));
            indices.push_back(static_cast<u32>(top_right));
            
            // Second triangle
            indices.push_back(static_cast<u32>(top_right));
            indices.push_back(static_cast<u32>(bottom_left));
            indices.push_back(static_cast<u32>(bottom_right));
        }
    }
    
    m_index_count = static_cast<u32>(indices.size());

    // Create VAO
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glGenBuffers(1, &m_ebo);

    glBindVertexArray(m_vao);

    // Upload vertex data
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, 
                 static_cast<GLsizeiptr>(vertices.size() * sizeof(float)), 
                 vertices.data(), 
                 GL_STATIC_DRAW);

    // Position attribute (location 0)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);

    // Texcoord attribute (location 1)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), 
                          (void*)(3 * sizeof(float)));

    // Upload indices
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, 
                 static_cast<GLsizeiptr>(indices.size() * sizeof(u32)), 
                 indices.data(), 
                 GL_STATIC_DRAW);

    glBindVertexArray(0);
}

void Water::draw() const {
    if (!m_vao) return;
    
    glBindVertexArray(m_vao);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(m_index_count), GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

} // namespace hz
