#include "mesh.hpp"

#include <cmath>

#include <glm/gtc/constants.hpp>

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

    // Tangent attribute (location 3) - for normal mapping
    gl::set_vertex_attrib({.index = 3,
                           .size = 3,
                           .type = GL_FLOAT,
                           .normalized = false,
                           .stride = sizeof(Vertex),
                           .offset = offsetof(Vertex, tangent)});

    // Bone IDs attribute (location 4) - for skeletal animation
    gl::set_vertex_attrib_int({.index = 4,
                               .size = MAX_BONE_INFLUENCE,
                               .type = GL_INT,
                               .stride = sizeof(Vertex),
                               .offset = offsetof(Vertex, bone_ids)});

    // Bone weights attribute (location 5) - for skeletal animation
    gl::set_vertex_attrib({.index = 5,
                           .size = MAX_BONE_INFLUENCE,
                           .type = GL_FLOAT,
                           .normalized = false,
                           .stride = sizeof(Vertex),
                           .offset = offsetof(Vertex, bone_weights)});

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

            // Tangent is along X axis for a horizontal plane
            Vertex v;
            v.position = {px, 0.0f, pz};
            v.normal = {0.0f, 1.0f, 0.0f};
            v.texcoord = {static_cast<f32>(x), static_cast<f32>(z)};
            v.tangent = {1.0f, 0.0f, 0.0f};
            // Explicitly init bone data
            for (int k = 0; k < 4; k++) {
                v.bone_ids[k] = -1;
                v.bone_weights[k] = 0.0f;
            }
            vertices.push_back(v);
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

    std::vector<Vertex> vertices;
    vertices.reserve(24);

    auto add_vertex = [&](glm::vec3 p, glm::vec3 n, glm::vec2 uv, glm::vec3 t) {
        Vertex v;
        v.position = p;
        v.normal = n;
        v.texcoord = uv;
        v.tangent = t;
        for (int k = 0; k < 4; k++) {
            v.bone_ids[k] = -1;
            v.bone_weights[k] = 0.0f;
        }
        vertices.push_back(v);
    };

    // Front face - tangent along +X
    add_vertex({-h, -h, h}, {0, 0, 1}, {0, 0}, {1, 0, 0});
    add_vertex({h, -h, h}, {0, 0, 1}, {1, 0}, {1, 0, 0});
    add_vertex({h, h, h}, {0, 0, 1}, {1, 1}, {1, 0, 0});
    add_vertex({-h, h, h}, {0, 0, 1}, {0, 1}, {1, 0, 0});

    // Back face - tangent along -X
    add_vertex({h, -h, -h}, {0, 0, -1}, {0, 0}, {-1, 0, 0});
    add_vertex({-h, -h, -h}, {0, 0, -1}, {1, 0}, {-1, 0, 0});
    add_vertex({-h, h, -h}, {0, 0, -1}, {1, 1}, {-1, 0, 0});
    add_vertex({h, h, -h}, {0, 0, -1}, {0, 1}, {-1, 0, 0});

    // Top face - tangent along +X
    add_vertex({-h, h, h}, {0, 1, 0}, {0, 0}, {1, 0, 0});
    add_vertex({h, h, h}, {0, 1, 0}, {1, 0}, {1, 0, 0});
    add_vertex({h, h, -h}, {0, 1, 0}, {1, 1}, {1, 0, 0});
    add_vertex({-h, h, -h}, {0, 1, 0}, {0, 1}, {1, 0, 0});

    // Bottom face - tangent along +X
    add_vertex({-h, -h, -h}, {0, -1, 0}, {0, 0}, {1, 0, 0});
    add_vertex({h, -h, -h}, {0, -1, 0}, {1, 0}, {1, 0, 0});
    add_vertex({h, -h, h}, {0, -1, 0}, {1, 1}, {1, 0, 0});
    add_vertex({-h, -h, h}, {0, -1, 0}, {0, 1}, {1, 0, 0});

    // Right face - tangent along -Z
    add_vertex({h, -h, h}, {1, 0, 0}, {0, 0}, {0, 0, -1});
    add_vertex({h, -h, -h}, {1, 0, 0}, {1, 0}, {0, 0, -1});
    add_vertex({h, h, -h}, {1, 0, 0}, {1, 1}, {0, 0, -1});
    add_vertex({h, h, h}, {1, 0, 0}, {0, 1}, {0, 0, -1});

    // Left face - tangent along +Z
    add_vertex({-h, -h, -h}, {-1, 0, 0}, {0, 0}, {0, 0, 1});
    add_vertex({-h, -h, h}, {-1, 0, 0}, {1, 0}, {0, 0, 1});
    add_vertex({-h, h, h}, {-1, 0, 0}, {1, 1}, {0, 0, 1});
    add_vertex({-h, h, -h}, {-1, 0, 0}, {0, 1}, {0, 0, 1});

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

Mesh Mesh::create_sphere(f32 radius, i32 slices, i32 stacks) {
    std::vector<Vertex> vertices;
    std::vector<u32> indices;

    f32 pi = glm::pi<f32>();
    f32 two_pi = 2.0f * pi;

    for (i32 i = 0; i <= stacks; ++i) {
        f32 v = static_cast<f32>(i) / static_cast<f32>(stacks);
        f32 phi = v * pi;

        for (i32 j = 0; j <= slices; ++j) {
            f32 u = static_cast<f32>(j) / static_cast<f32>(slices);
            f32 theta = u * two_pi;

            f32 x = cos(theta) * sin(phi);
            f32 y = cos(phi);
            f32 z = sin(theta) * sin(phi);

            glm::vec3 pos = glm::vec3(x, y, z) * radius;
            glm::vec3 normal = glm::vec3(x, y, z);
            glm::vec2 texcoord = glm::vec2(u, v);

            // Tangent calculation for sphere
            // Tangent is perpendicular to normal and up (0,1,0), or derived from theta derivative
            // Simple approximation: -sin(theta), 0, cos(theta)
            glm::vec3 tangent = glm::normalize(glm::vec3(-sin(theta), 0.0f, cos(theta)));

            vertices.push_back({pos, normal, texcoord, tangent});
        }
    }

    for (i32 i = 0; i < stacks; ++i) {
        for (i32 j = 0; j < slices; ++j) {
            u32 first = (i * (slices + 1)) + j;
            u32 second = first + slices + 1;

            indices.push_back(first);
            indices.push_back(second);
            indices.push_back(first + 1);

            indices.push_back(second);
            indices.push_back(second + 1);
            indices.push_back(first + 1);
        }
    }

    return Mesh(std::move(vertices), std::move(indices));
}

void Mesh::setup_instancing(const std::vector<glm::mat4>& instance_transforms) {
    m_instance_count = static_cast<u32>(instance_transforms.size());
    if (m_instance_count == 0)
        return;

    m_vao.bind();

    // Upload instance data to VBO
    // Reinterpreting cast to byte span for generic buffer upload if needed,
    // but assuming set_data handles span<T> correctly by sizeof(T) * size()
    m_instance_vbo.set_data(std::span<const glm::mat4>(instance_transforms));

    // Matrix attributes (location 6, 7, 8, 9)
    // A mat4 is 4 vec4s
    std::size_t vec4_size = sizeof(glm::vec4);
    for (int i = 0; i < 4; ++i) {
        glEnableVertexAttribArray(6 + i);
        glVertexAttribPointer(6 + i, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4),
                              (void*)(i * vec4_size));
        glVertexAttribDivisor(6 + i, 1); // Tell OpenGL this is an instanced attribute
    }

    gl::VertexArray::unbind();
}

void Mesh::draw_instanced(u32 instance_count) const {
    m_vao.bind();
    glDrawElementsInstanced(GL_TRIANGLES, static_cast<GLsizei>(m_index_count), GL_UNSIGNED_INT,
                            nullptr, static_cast<GLsizei>(instance_count));
}

} // namespace hz
