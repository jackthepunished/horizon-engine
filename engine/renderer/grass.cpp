#include "grass.hpp"

#include "engine/core/log.hpp"
#include "opengl/gl_context.hpp"

#include <cstddef> // for offsetof
#include <random>

#include <glm/gtc/constants.hpp>

namespace hz {

Grass::~Grass() noexcept {
    if (m_vao)
        glDeleteVertexArrays(1, &m_vao);
    if (m_vbo)
        glDeleteBuffers(1, &m_vbo);
    if (m_instance_vbo)
        glDeleteBuffers(1, &m_instance_vbo);
}

void Grass::generate(const Terrain& terrain, const GrassConfig& config, u32 seed) {
    m_config = config;
    m_instances.clear();
    m_instances.reserve(config.blade_count);

    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> dist_x(-terrain.width() / 2.0f, terrain.width() / 2.0f);
    std::uniform_real_distribution<float> dist_z(-terrain.depth() / 2.0f, terrain.depth() / 2.0f);
    std::uniform_real_distribution<float> dist_height(config.min_height, config.max_height);
    std::uniform_real_distribution<float> dist_rotation(0.0f, glm::two_pi<float>());
    std::uniform_real_distribution<float> dist_color(0.0f, 1.0f);

    for (u32 i = 0; i < config.blade_count; ++i) {
        float x = dist_x(rng);
        float z = dist_z(rng);
        float y =
            terrain.get_height_at(x, z) - 5.0f - 0.2f; // Match terrain offset + embed slightly

        GrassInstance instance;
        instance.position = glm::vec3(x, y, z);
        instance.height = dist_height(rng);
        instance.rotation = dist_rotation(rng);
        instance.color_variation = dist_color(rng);

        m_instances.push_back(instance);
    }

    create_blade_mesh();
    upload_instances();

    HZ_ENGINE_INFO("Generated {} grass blades on terrain", m_instances.size());
}

void Grass::create_blade_mesh() {
    // Simple quad for grass blade (2 triangles)
    // Vertices: position (3) + texcoord (2)
    float blade_vertices[] = {
        // First triangle
        -0.5f, 0.0f, 0.0f, 0.0f, 0.0f, // Bottom left (UV: 0,0)
        0.5f, 0.0f, 0.0f, 1.0f, 0.0f,  // Bottom right (UV: 1,0)
        0.5f, 1.0f, 0.0f, 1.0f, 1.0f,  // Top right (UV: 1,1)
        // Second triangle
        -0.5f, 0.0f, 0.0f, 0.0f, 0.0f, // Bottom left
        0.5f, 1.0f, 0.0f, 1.0f, 1.0f,  // Top right
        -0.5f, 1.0f, 0.0f, 0.0f, 1.0f, // Top left (UV: 0,1)
    };

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);

    glBindVertexArray(m_vao);

    // Blade mesh VBO
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(blade_vertices), blade_vertices, GL_STATIC_DRAW);

    // Position attribute (location 0)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);

    // Texcoord attribute (location 1)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));

    glBindVertexArray(0);
}

void Grass::upload_instances() {
    if (m_instances.empty())
        return;

    glBindVertexArray(m_vao);

    if (!m_instance_vbo) {
        glGenBuffers(1, &m_instance_vbo);
    }

    glBindBuffer(GL_ARRAY_BUFFER, m_instance_vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(m_instances.size() * sizeof(GrassInstance)),
                 m_instances.data(), GL_STATIC_DRAW);

    // Instance position (location 2)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(GrassInstance),
                          (void*)offsetof(GrassInstance, position));
    glVertexAttribDivisor(2, 1);

    // Instance height (location 3)
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(GrassInstance),
                          (void*)offsetof(GrassInstance, height));
    glVertexAttribDivisor(3, 1);

    // Instance rotation (location 4)
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(GrassInstance),
                          (void*)offsetof(GrassInstance, rotation));
    glVertexAttribDivisor(4, 1);

    // Instance color variation (location 5)
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, sizeof(GrassInstance),
                          (void*)offsetof(GrassInstance, color_variation));
    glVertexAttribDivisor(5, 1);

    glBindVertexArray(0);
}

void Grass::draw(float time) const {
    if (m_vao == 0 || m_instances.empty())
        return;

    HZ_UNUSED(time); // Wind animation is handled in shader via uniform

    glBindVertexArray(m_vao);
    glDrawArraysInstanced(GL_TRIANGLES, 0, 6, static_cast<GLsizei>(m_instances.size()));
    glBindVertexArray(0);
}

} // namespace hz
