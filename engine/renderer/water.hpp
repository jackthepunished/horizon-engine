#pragma once

/**
 * @file water.hpp
 * @brief Realistic water rendering with reflections, refractions, and waves
 */

#include "engine/core/types.hpp"
#include "engine/rhi/rhi_command_list.hpp"
#include "engine/rhi/rhi_device.hpp"
#include "engine/rhi/rhi_resources.hpp"

#include <memory>

#include <glm/glm.hpp>

namespace hz {

/**
 * @brief Water vertex with position and texture coordinates
 */
struct WaterVertex {
    glm::vec3 position;
    glm::vec2 texcoord;
};

/**
 * @brief Water plane configuration
 */
struct WaterConfig {
    float size{100.0f};               // Size of water plane
    float height{0.0f};               // Y position of water surface
    float wave_strength{0.3f};        // Wave height
    float wave_speed{1.0f};           // Wave animation speed
    float distortion_strength{0.02f}; // Reflection/refraction distortion
    float transparency{0.8f};         // Water transparency
    float shine_damper{20.0f};        // Specular power
    float reflectivity{0.6f};         // Specular strength
    float depth_multiplier{0.1f};     // Depth color blending strength

    glm::vec3 water_color{0.0f, 0.3f, 0.5f};         // Deep water color
    glm::vec3 water_color_shallow{0.0f, 0.5f, 0.7f}; // Shallow water color
};

/**
 * @brief Water surface rendering with reflection and refraction
 */
class Water {
public:
    Water() = default;
    ~Water() = default;

    HZ_NON_COPYABLE(Water);
    HZ_DEFAULT_MOVABLE(Water);

    /**
     * @brief Initialize water plane
     * @param config Water configuration
     * @param device RHI device for buffer creation
     */
    void init(const WaterConfig& config, rhi::Device& device);

    /**
     * @brief Draw water surface
     * @param cmd Command list to record into
     */
    void draw(rhi::CommandList& cmd) const;

    /**
     * @brief Check if water is initialized
     */
    [[nodiscard]] bool is_valid() const { return m_vertex_buffer != nullptr; }

    /**
     * @brief Get configuration
     */
    [[nodiscard]] const WaterConfig& config() const { return m_config; }

    /**
     * @brief Update configuration
     */
    void set_config(const WaterConfig& config) { m_config = config; }

    /**
     * @brief Get water height (Y position)
     */
    [[nodiscard]] float height() const { return m_config.height; }

    /**
     * @brief Set water height
     */
    void set_height(float h) { m_config.height = h; }

private:
    void create_mesh(rhi::Device& device);

    WaterConfig m_config;

    // RHI buffers
    std::unique_ptr<rhi::Buffer> m_vertex_buffer;
    std::unique_ptr<rhi::Buffer> m_index_buffer;
    u32 m_index_count{0};
};

} // namespace hz
