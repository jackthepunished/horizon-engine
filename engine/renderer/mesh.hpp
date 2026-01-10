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

// Maximum bones influencing a single vertex (must match shader)
constexpr int MAX_BONE_INFLUENCE = 4;

/**
 * @brief Vertex structure with skeletal animation support
 */
struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texcoord;
    glm::vec3 tangent; // For normal mapping (TBN matrix)

    // Skeletal animation data
    int bone_ids[MAX_BONE_INFLUENCE] = {-1, -1, -1, -1};
    float bone_weights[MAX_BONE_INFLUENCE] = {0.0f, 0.0f, 0.0f, 0.0f};

    /**
     * @brief Add a bone influence to this vertex
     */
    void add_bone(int bone_id, float weight) {
        for (int i = 0; i < MAX_BONE_INFLUENCE; ++i) {
            if (bone_ids[i] < 0) {
                bone_ids[i] = bone_id;
                bone_weights[i] = weight;
                return;
            }
        }
        // All slots full - could replace lowest weight
    }

    /**
     * @brief Reset bone data
     */
    void reset_bones() {
        for (int i = 0; i < MAX_BONE_INFLUENCE; ++i) {
            bone_ids[i] = -1;
            bone_weights[i] = 0.0f;
        }
    }
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
