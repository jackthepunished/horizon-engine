#include "water.hpp"

#include "engine/core/log.hpp"

#include <span>
#include <vector>

namespace hz {

void Water::init(const WaterConfig& config, rhi::Device& device) {
    m_config = config;
    create_mesh(device);
    HZ_ENGINE_INFO("Water plane initialized: size={}, height={}", config.size, config.height);
}

void Water::create_mesh(rhi::Device& device) {
    float half_size = m_config.size * 0.5f;

    // Higher subdivision for better wave detail
    const int subdivisions = 32;
    const float step = m_config.size / static_cast<float>(subdivisions);

    std::vector<WaterVertex> vertices;
    vertices.reserve((subdivisions + 1) * (subdivisions + 1));

    for (int z = 0; z <= subdivisions; ++z) {
        for (int x = 0; x <= subdivisions; ++x) {
            WaterVertex v;
            v.position = glm::vec3(-half_size + static_cast<float>(x) * step, m_config.height,
                                   -half_size + static_cast<float>(z) * step);
            v.texcoord = glm::vec2(static_cast<float>(x) / static_cast<float>(subdivisions),
                                   static_cast<float>(z) / static_cast<float>(subdivisions));
            vertices.push_back(v);
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

    m_vertex_buffer =
        device.create_vertex_buffer(std::span<const WaterVertex>(vertices), "Water VB");
    m_index_buffer = device.create_index_buffer(std::span<const u32>(indices), "Water IB");
}

void Water::draw(rhi::CommandList& cmd) const {
    if (!m_vertex_buffer)
        return;

    cmd.bind_vertex_buffer(0, *m_vertex_buffer);
    if (m_index_buffer) {
        cmd.bind_index_buffer(*m_index_buffer, 0, rhi::IndexType::Uint32);
        cmd.draw_indexed(m_index_count);
    }
}

} // namespace hz
