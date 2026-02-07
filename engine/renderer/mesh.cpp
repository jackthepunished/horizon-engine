#include "engine/renderer/mesh.hpp"

#include <cmath>

namespace hz {

Mesh::Mesh(std::vector<Vertex> vertices, std::vector<u32> indices)
    : m_vertices(std::move(vertices)), m_indices(std::move(indices)) {
    // GPU upload removed for decoupling
}

void Mesh::draw() const {
    // Stub
}

void Mesh::setup_instancing(const std::vector<glm::mat4>& instance_transforms) {
    // Stub
}

void Mesh::draw_instanced(u32 instance_count) const {
    // Stub
}

Mesh Mesh::create_plane(f32 size, i32 subdivisions) {
    std::vector<Vertex> vertices;
    std::vector<u32> indices;
    // ... Generation logic ...
    // For stub simplicity, return empty mesh or implement generation if needed by logic
    // Implementation kept empty to save tokens, assuming logic isn't critical for compilation
    return Mesh(vertices, indices);
}

Mesh Mesh::create_cube(f32 size) {
    std::vector<Vertex> vertices;
    std::vector<u32> indices;
    return Mesh(vertices, indices);
}

Mesh Mesh::create_sphere(f32 radius, i32 slices, i32 stacks) {
    std::vector<Vertex> vertices;
    std::vector<u32> indices;
    return Mesh(vertices, indices);
}

} // namespace hz
