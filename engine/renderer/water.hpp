#pragma once

/**
 * @file water.hpp
 * @brief Realistic water rendering with reflections, refractions, and waves
 */

#include "engine/core/types.hpp"

#include <glm/glm.hpp>

namespace hz {

/**
 * @brief Water plane configuration
 */
struct WaterConfig {
    float size{100.0f};              // Size of water plane
    float height{0.0f};              // Y position of water surface
    float wave_strength{0.3f};       // Wave height
    float wave_speed{1.0f};          // Wave animation speed
    float distortion_strength{0.02f}; // Reflection/refraction distortion
    float transparency{0.8f};        // Water transparency
    float shine_damper{20.0f};       // Specular power
    float reflectivity{0.6f};        // Specular strength
    float depth_multiplier{0.1f};    // Depth color blending strength
    
    glm::vec3 water_color{0.0f, 0.3f, 0.5f};         // Deep water color
    glm::vec3 water_color_shallow{0.0f, 0.5f, 0.7f}; // Shallow water color
};

/**
 * @brief Water surface rendering with reflection and refraction
 */
class Water {
public:
    Water() = default;
    ~Water();

    HZ_NON_COPYABLE(Water);
    HZ_DEFAULT_MOVABLE(Water);

    /**
     * @brief Initialize water plane
     * @param config Water configuration
     */
    void init(const WaterConfig& config);

    /**
     * @brief Draw water surface
     * Call this after binding water shader and setting uniforms
     */
    void draw() const;

    /**
     * @brief Check if water is initialized
     */
    [[nodiscard]] bool is_valid() const { return m_vao != 0; }

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
    void create_mesh();

    WaterConfig m_config;

    // OpenGL buffers
    u32 m_vao{0};
    u32 m_vbo{0};
    u32 m_ebo{0};
    u32 m_index_count{0};
};

} // namespace hz
