#include "engine/renderer/mesh.hpp"

#include "engine/core/log.hpp"

#include <cmath>
#include <numbers>
#include <span>

namespace hz {

// ============================================================================
// Construction
// ============================================================================

Mesh::Mesh(std::vector<Vertex> vertices, std::vector<u32> indices)
    : m_vertices(std::move(vertices)), m_indices(std::move(indices)) {}

glm::vec4 Mesh::bounding_sphere() const {
    if (m_bounds_cached) return m_bounding_sphere;
    if (m_vertices.empty()) {
        m_bounding_sphere = glm::vec4(0.0f);
        m_bounds_cached = true;
        return m_bounding_sphere;
    }
    // Centroid then max-distance radius. Cheap and good enough for frustum culling.
    glm::vec3 center(0.0f);
    for (const auto& v : m_vertices) center += v.position;
    center /= static_cast<float>(m_vertices.size());
    float r2 = 0.0f;
    for (const auto& v : m_vertices) {
        const glm::vec3 d = v.position - center;
        r2 = std::max(r2, glm::dot(d, d));
    }
    m_bounding_sphere = glm::vec4(center, std::sqrt(r2));
    m_bounds_cached = true;
    return m_bounding_sphere;
}

// ============================================================================
// GPU Resource Management
// ============================================================================

void Mesh::upload_to_gpu(rhi::Device& device, const char* debug_name) {
    if (m_vertices.empty()) {
        HZ_LOG_WARN("Mesh::upload_to_gpu called with empty vertex data");
        return;
    }

    // Build debug names
    std::string vb_name = debug_name ? std::string(debug_name) + " VB" : "Mesh VB";
    std::string ib_name = debug_name ? std::string(debug_name) + " IB" : "Mesh IB";

    m_vertex_buffer =
        device.create_vertex_buffer(std::span<const Vertex>(m_vertices), vb_name.c_str());

    if (!m_indices.empty()) {
        m_index_buffer =
            device.create_index_buffer(std::span<const u32>(m_indices), ib_name.c_str());
    }
}

void Mesh::release_gpu_resources() {
    m_vertex_buffer.reset();
    m_index_buffer.reset();
}

// ============================================================================
// Drawing
// ============================================================================

void Mesh::draw(rhi::CommandList& cmd) const {
    if (!m_vertex_buffer) {
        return;
    }

    cmd.bind_vertex_buffer(0, *m_vertex_buffer);

    if (m_index_buffer && !m_indices.empty()) {
        cmd.bind_index_buffer(*m_index_buffer, 0, rhi::IndexType::Uint32);
        cmd.draw_indexed(static_cast<u32>(m_indices.size()));
    } else {
        cmd.draw(static_cast<u32>(m_vertices.size()));
    }
}

void Mesh::draw_instanced(rhi::CommandList& cmd, u32 instance_count) const {
    if (!m_vertex_buffer) {
        return;
    }

    cmd.bind_vertex_buffer(0, *m_vertex_buffer);

    if (m_index_buffer && !m_indices.empty()) {
        cmd.bind_index_buffer(*m_index_buffer, 0, rhi::IndexType::Uint32);
        cmd.draw_indexed(static_cast<u32>(m_indices.size()), instance_count);
    } else {
        cmd.draw(static_cast<u32>(m_vertices.size()), instance_count);
    }
}

// ============================================================================
// Helper: compute tangent for a triangle
// ============================================================================

static void calculate_tangent(Vertex& v0, Vertex& v1, Vertex& v2) {
    glm::vec3 edge1 = v1.position - v0.position;
    glm::vec3 edge2 = v2.position - v0.position;
    glm::vec2 duv1 = v1.texcoord - v0.texcoord;
    glm::vec2 duv2 = v2.texcoord - v0.texcoord;

    float denom = duv1.x * duv2.y - duv2.x * duv1.y;
    float f = (std::abs(denom) > 1e-6f) ? 1.0f / denom : 0.0f;

    glm::vec3 tangent;
    tangent.x = f * (duv2.y * edge1.x - duv1.y * edge2.x);
    tangent.y = f * (duv2.y * edge1.y - duv1.y * edge2.y);
    tangent.z = f * (duv2.y * edge1.z - duv1.y * edge2.z);

    float len = glm::length(tangent);
    if (len > 1e-6f) {
        tangent /= len;
    } else {
        tangent = glm::vec3(1.0f, 0.0f, 0.0f);
    }

    // Compute handedness
    glm::vec3 bitangent;
    bitangent.x = f * (-duv2.x * edge1.x + duv1.x * edge2.x);
    bitangent.y = f * (-duv2.x * edge1.y + duv1.x * edge2.y);
    bitangent.z = f * (-duv2.x * edge1.z + duv1.x * edge2.z);

    float handedness = (glm::dot(glm::cross(v0.normal, tangent), bitangent) < 0.0f) ? -1.0f : 1.0f;

    glm::vec4 t = glm::vec4(tangent, handedness);
    v0.tangent = t;
    v1.tangent = t;
    v2.tangent = t;
}

// ============================================================================
// Plane Generation
// ============================================================================

Mesh Mesh::create_plane(f32 size, i32 subdivisions) {
    // Clamp subdivisions to at least 1
    if (subdivisions < 1)
        subdivisions = 1;

    const i32 verts_per_side = subdivisions + 1;
    const f32 half = size * 0.5f;
    const f32 step = size / static_cast<f32>(subdivisions);

    std::vector<Vertex> vertices;
    vertices.reserve(static_cast<size_t>(verts_per_side * verts_per_side));

    std::vector<u32> indices;
    indices.reserve(static_cast<size_t>(subdivisions * subdivisions * 6));

    // Generate vertices on XZ plane, Y up
    for (i32 z = 0; z < verts_per_side; ++z) {
        for (i32 x = 0; x < verts_per_side; ++x) {
            Vertex v{};
            v.position = {-half + static_cast<f32>(x) * step, 0.0f,
                          -half + static_cast<f32>(z) * step};
            v.normal = {0.0f, 1.0f, 0.0f};
            v.texcoord = {static_cast<f32>(x) / static_cast<f32>(subdivisions),
                          static_cast<f32>(z) / static_cast<f32>(subdivisions)};
            v.tangent = {1.0f, 0.0f, 0.0f, 1.0f}; // Tangent along +X
            vertices.push_back(v);
        }
    }

    // Generate indices (two triangles per quad)
    for (i32 z = 0; z < subdivisions; ++z) {
        for (i32 x = 0; x < subdivisions; ++x) {
            u32 tl = static_cast<u32>(z * verts_per_side + x);
            u32 tr = tl + 1;
            u32 bl = tl + static_cast<u32>(verts_per_side);
            u32 br = bl + 1;

            // CCW winding when viewed from +Y
            indices.push_back(tl);
            indices.push_back(bl);
            indices.push_back(tr);

            indices.push_back(tr);
            indices.push_back(bl);
            indices.push_back(br);
        }
    }

    return Mesh(std::move(vertices), std::move(indices));
}

// ============================================================================
// Cube Generation
// ============================================================================

Mesh Mesh::create_cube(f32 size) {
    const f32 h = size * 0.5f;

    // 24 vertices (4 per face, 6 faces) for proper per-face normals
    // clang-format off
    std::vector<Vertex> vertices = {
        // Front face (+Z) — normal (0, 0, 1)
        {{-h, -h,  h}, { 0,  0,  1}, {0, 0}, {1, 0, 0, 1}, {-1,-1,-1,-1}, {0,0,0,0}},
        {{ h, -h,  h}, { 0,  0,  1}, {1, 0}, {1, 0, 0, 1}, {-1,-1,-1,-1}, {0,0,0,0}},
        {{ h,  h,  h}, { 0,  0,  1}, {1, 1}, {1, 0, 0, 1}, {-1,-1,-1,-1}, {0,0,0,0}},
        {{-h,  h,  h}, { 0,  0,  1}, {0, 1}, {1, 0, 0, 1}, {-1,-1,-1,-1}, {0,0,0,0}},

        // Back face (-Z) — normal (0, 0, -1)
        {{ h, -h, -h}, { 0,  0, -1}, {0, 0}, {-1, 0, 0, 1}, {-1,-1,-1,-1}, {0,0,0,0}},
        {{-h, -h, -h}, { 0,  0, -1}, {1, 0}, {-1, 0, 0, 1}, {-1,-1,-1,-1}, {0,0,0,0}},
        {{-h,  h, -h}, { 0,  0, -1}, {1, 1}, {-1, 0, 0, 1}, {-1,-1,-1,-1}, {0,0,0,0}},
        {{ h,  h, -h}, { 0,  0, -1}, {0, 1}, {-1, 0, 0, 1}, {-1,-1,-1,-1}, {0,0,0,0}},

        // Top face (+Y) — normal (0, 1, 0)
        {{-h,  h,  h}, { 0,  1,  0}, {0, 0}, {1, 0, 0, 1}, {-1,-1,-1,-1}, {0,0,0,0}},
        {{ h,  h,  h}, { 0,  1,  0}, {1, 0}, {1, 0, 0, 1}, {-1,-1,-1,-1}, {0,0,0,0}},
        {{ h,  h, -h}, { 0,  1,  0}, {1, 1}, {1, 0, 0, 1}, {-1,-1,-1,-1}, {0,0,0,0}},
        {{-h,  h, -h}, { 0,  1,  0}, {0, 1}, {1, 0, 0, 1}, {-1,-1,-1,-1}, {0,0,0,0}},

        // Bottom face (-Y) — normal (0, -1, 0)
        {{-h, -h, -h}, { 0, -1,  0}, {0, 0}, {1, 0, 0, 1}, {-1,-1,-1,-1}, {0,0,0,0}},
        {{ h, -h, -h}, { 0, -1,  0}, {1, 0}, {1, 0, 0, 1}, {-1,-1,-1,-1}, {0,0,0,0}},
        {{ h, -h,  h}, { 0, -1,  0}, {1, 1}, {1, 0, 0, 1}, {-1,-1,-1,-1}, {0,0,0,0}},
        {{-h, -h,  h}, { 0, -1,  0}, {0, 1}, {1, 0, 0, 1}, {-1,-1,-1,-1}, {0,0,0,0}},

        // Right face (+X) — normal (1, 0, 0)
        {{ h, -h,  h}, { 1,  0,  0}, {0, 0}, {0, 0, -1, 1}, {-1,-1,-1,-1}, {0,0,0,0}},
        {{ h, -h, -h}, { 1,  0,  0}, {1, 0}, {0, 0, -1, 1}, {-1,-1,-1,-1}, {0,0,0,0}},
        {{ h,  h, -h}, { 1,  0,  0}, {1, 1}, {0, 0, -1, 1}, {-1,-1,-1,-1}, {0,0,0,0}},
        {{ h,  h,  h}, { 1,  0,  0}, {0, 1}, {0, 0, -1, 1}, {-1,-1,-1,-1}, {0,0,0,0}},

        // Left face (-X) — normal (-1, 0, 0)
        {{-h, -h, -h}, {-1,  0,  0}, {0, 0}, {0, 0, 1, 1}, {-1,-1,-1,-1}, {0,0,0,0}},
        {{-h, -h,  h}, {-1,  0,  0}, {1, 0}, {0, 0, 1, 1}, {-1,-1,-1,-1}, {0,0,0,0}},
        {{-h,  h,  h}, {-1,  0,  0}, {1, 1}, {0, 0, 1, 1}, {-1,-1,-1,-1}, {0,0,0,0}},
        {{-h,  h, -h}, {-1,  0,  0}, {0, 1}, {0, 0, 1, 1}, {-1,-1,-1,-1}, {0,0,0,0}},
    };
    // clang-format on

    std::vector<u32> indices;
    indices.reserve(36);

    // 6 faces, each with 2 triangles (CCW)
    for (u32 face = 0; face < 6; ++face) {
        u32 base = face * 4;
        indices.push_back(base + 0);
        indices.push_back(base + 1);
        indices.push_back(base + 2);
        indices.push_back(base + 0);
        indices.push_back(base + 2);
        indices.push_back(base + 3);
    }

    return Mesh(std::move(vertices), std::move(indices));
}

// ============================================================================
// Sphere Generation (UV Sphere)
// ============================================================================

Mesh Mesh::create_sphere(f32 radius, i32 slices, i32 stacks) {
    constexpr f32 PI = std::numbers::pi_v<f32>;

    std::vector<Vertex> vertices;
    vertices.reserve(static_cast<size_t>((stacks + 1) * (slices + 1)));

    std::vector<u32> indices;
    indices.reserve(static_cast<size_t>(stacks * slices * 6));

    for (i32 stack = 0; stack <= stacks; ++stack) {
        f32 phi = PI * static_cast<f32>(stack) / static_cast<f32>(stacks); // 0 to PI
        f32 sin_phi = std::sin(phi);
        f32 cos_phi = std::cos(phi);

        for (i32 slice = 0; slice <= slices; ++slice) {
            f32 theta = 2.0f * PI * static_cast<f32>(slice) / static_cast<f32>(slices);
            f32 sin_theta = std::sin(theta);
            f32 cos_theta = std::cos(theta);

            Vertex v{};
            v.normal = {sin_phi * cos_theta, cos_phi, sin_phi * sin_theta};
            v.position = v.normal * radius;
            v.texcoord = {static_cast<f32>(slice) / static_cast<f32>(slices),
                          static_cast<f32>(stack) / static_cast<f32>(stacks)};

            // Tangent is derivative of position w.r.t. theta
            glm::vec3 tangent = {-sin_theta, 0.0f, cos_theta};
            v.tangent = glm::vec4(tangent, 1.0f);

            vertices.push_back(v);
        }
    }

    // Generate indices
    for (i32 stack = 0; stack < stacks; ++stack) {
        for (i32 slice = 0; slice < slices; ++slice) {
            u32 current = static_cast<u32>(stack * (slices + 1) + slice);
            u32 next = current + static_cast<u32>(slices + 1);

            // CCW winding
            indices.push_back(current);
            indices.push_back(next);
            indices.push_back(current + 1);

            indices.push_back(current + 1);
            indices.push_back(next);
            indices.push_back(next + 1);
        }
    }

    // Calculate tangents per-triangle for better accuracy at poles
    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
        calculate_tangent(vertices[indices[i]], vertices[indices[i + 1]], vertices[indices[i + 2]]);
    }

    return Mesh(std::move(vertices), std::move(indices));
}

} // namespace hz
