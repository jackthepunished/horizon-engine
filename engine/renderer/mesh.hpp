#pragma once

/**
 * @file mesh.hpp
 * @brief Basic mesh class for OpenGL rendering
 */

#include "engine/core/types.hpp"
#include "opengl/buffer.hpp"

#include <vector>

#include <glm/glm.hpp>

namespace hz {

/**
 * @brief Simple vertex structure for basic rendering
 */
struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texcoord;
    glm::vec3 tangent; // For normal mapping (TBN matrix)
};

/**
 * @brief Basic mesh class with VAO/VBO/EBO
 */
class Mesh {
public:
    Mesh(std::vector<Vertex> vertices, std::vector<u32> indices);
    ~Mesh() = default;

    HZ_NON_COPYABLE(Mesh);
    HZ_DEFAULT_MOVABLE(Mesh);

    /**
     * @brief Draw the mesh
     */
    void draw() const;

    /**
     * @brief Create a ground plane mesh
     */
    static Mesh create_plane(f32 size = 20.0f, i32 subdivisions = 10);

    /**
     * @brief Create a cube mesh
     */
    static Mesh create_cube(f32 size = 1.0f);

    /**
     * @brief Create a sphere mesh
     */
    static Mesh create_sphere(f32 radius, i32 slices = 32, i32 stacks = 16);

private:
    gl::VertexArray m_vao;
    gl::VertexBuffer m_vbo;
    gl::IndexBuffer m_ebo;
    u32 m_index_count{0};
};

} // namespace hz
