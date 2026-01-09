#include "mesh.hpp"

namespace hz {

Mesh::Mesh(std::vector<Vertex> vertices, std::vector<u32> indices) {
    m_index_count = static_cast<u32>(indices.size());

    m_vao.bind();

    m_vbo.set_data(std::span<const Vertex>(vertices));
    m_ebo.set_data(std::span<const u32>(indices));

    // Position attribute (location 0)
    gl::set_vertex_attrib({.index = 0,
                           .size = 3,
                           .type = GL_FLOAT,
                           .normalized = false,
                           .stride = sizeof(Vertex),
                           .offset = offsetof(Vertex, position)});

    // Normal attribute (location 1)
    gl::set_vertex_attrib({.index = 1,
                           .size = 3,
                           .type = GL_FLOAT,
                           .normalized = false,
                           .stride = sizeof(Vertex),
                           .offset = offsetof(Vertex, normal)});

    // Texcoord attribute (location 2)
    gl::set_vertex_attrib({.index = 2,
                           .size = 2,
                           .type = GL_FLOAT,
                           .normalized = false,
                           .stride = sizeof(Vertex),
                           .offset = offsetof(Vertex, texcoord)});

    gl::VertexArray::unbind();
}

void Mesh::draw() const {
    m_vao.bind();
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(m_index_count), GL_UNSIGNED_INT, nullptr);
}

Mesh Mesh::create_plane(f32 size, i32 subdivisions) {
    std::vector<Vertex> vertices;
    std::vector<u32> indices;

    f32 half_size = size / 2.0f;
    f32 step = size / static_cast<f32>(subdivisions);

    // Generate vertices in a grid pattern
    for (i32 z = 0; z <= subdivisions; ++z) {
        for (i32 x = 0; x <= subdivisions; ++x) {
            f32 px = -half_size + static_cast<f32>(x) * step;
            f32 pz = -half_size + static_cast<f32>(z) * step;

            vertices.push_back({.position = {px, 0.0f, pz},
                                .normal = {0.0f, 1.0f, 0.0f},
                                .texcoord = {static_cast<f32>(x), static_cast<f32>(z)}});
        }
    }

    // Generate indices for triangles
    for (i32 z = 0; z < subdivisions; ++z) {
        for (i32 x = 0; x < subdivisions; ++x) {
            u32 top_left = static_cast<u32>(z * (subdivisions + 1) + x);
            u32 top_right = top_left + 1;
            u32 bottom_left = static_cast<u32>((z + 1) * (subdivisions + 1) + x);
            u32 bottom_right = bottom_left + 1;

            // First triangle
            indices.push_back(top_left);
            indices.push_back(bottom_left);
            indices.push_back(top_right);

            // Second triangle
            indices.push_back(top_right);
            indices.push_back(bottom_left);
            indices.push_back(bottom_right);
        }
    }

    return Mesh(std::move(vertices), std::move(indices));
}

Mesh Mesh::create_cube(f32 size) {
    f32 h = size / 2.0f;

    std::vector<Vertex> vertices = {
        // Front
        {{-h, -h, h}, {0, 0, 1}, {0, 0}},
        {{h, -h, h}, {0, 0, 1}, {1, 0}},
        {{h, h, h}, {0, 0, 1}, {1, 1}},
        {{-h, h, h}, {0, 0, 1}, {0, 1}},
        // Back
        {{h, -h, -h}, {0, 0, -1}, {0, 0}},
        {{-h, -h, -h}, {0, 0, -1}, {1, 0}},
        {{-h, h, -h}, {0, 0, -1}, {1, 1}},
        {{h, h, -h}, {0, 0, -1}, {0, 1}},
        // Top
        {{-h, h, h}, {0, 1, 0}, {0, 0}},
        {{h, h, h}, {0, 1, 0}, {1, 0}},
        {{h, h, -h}, {0, 1, 0}, {1, 1}},
        {{-h, h, -h}, {0, 1, 0}, {0, 1}},
        // Bottom
        {{-h, -h, -h}, {0, -1, 0}, {0, 0}},
        {{h, -h, -h}, {0, -1, 0}, {1, 0}},
        {{h, -h, h}, {0, -1, 0}, {1, 1}},
        {{-h, -h, h}, {0, -1, 0}, {0, 1}},
        // Right
        {{h, -h, h}, {1, 0, 0}, {0, 0}},
        {{h, -h, -h}, {1, 0, 0}, {1, 0}},
        {{h, h, -h}, {1, 0, 0}, {1, 1}},
        {{h, h, h}, {1, 0, 0}, {0, 1}},
        // Left
        {{-h, -h, -h}, {-1, 0, 0}, {0, 0}},
        {{-h, -h, h}, {-1, 0, 0}, {1, 0}},
        {{-h, h, h}, {-1, 0, 0}, {1, 1}},
        {{-h, h, -h}, {-1, 0, 0}, {0, 1}},
    };

    std::vector<u32> indices;
    for (u32 face = 0; face < 6; ++face) {
        u32 base = face * 4;
        indices.push_back(base);
        indices.push_back(base + 1);
        indices.push_back(base + 2);
        indices.push_back(base);
        indices.push_back(base + 2);
        indices.push_back(base + 3);
    }

    return Mesh(std::move(vertices), std::move(indices));
}

} // namespace hz
