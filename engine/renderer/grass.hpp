#pragma once

/**
 * @file grass.hpp
 * @brief Billboard grass rendering system with instanced rendering
 */

#include "engine/core/types.hpp"
#include "engine/renderer/terrain.hpp"

#include <vector>

#include <glm/glm.hpp>

namespace hz {

/**
 * @brief Grass rendering configuration
 */
struct GrassConfig {
    u32 blade_count{50000};       // Number of grass blades
    float min_height{0.3f};       // Minimum blade height
    float max_height{0.8f};       // Maximum blade height
    float wind_strength{0.3f};    // Wind animation strength
    float wind_speed{1.5f};       // Wind animation speed
    float density_falloff{50.0f}; // Distance at which grass fades out
    float blade_width{0.1f};      // Width of grass blade quad
};

/**
 * @brief Instance data for a single grass blade
 */
struct GrassInstance {
    glm::vec3 position;    // World position
    float height;          // Blade height
    float rotation;        // Y-axis rotation (radians)
    float color_variation; // Color tint variation [0-1]
};

/**
 * @brief Billboard grass rendering system with instanced rendering
 */
class Grass {
public:
    Grass() = default;
    ~Grass();

    HZ_NON_COPYABLE(Grass);
    HZ_DEFAULT_MOVABLE(Grass);

    /**
     * @brief Generate grass blades on terrain
     * @param terrain The terrain to place grass on
     * @param config Grass configuration
     * @param seed Random seed for placement
     */
    void generate(const Terrain& terrain, const GrassConfig& config, u32 seed = 0);

    /**
     * @brief Draw all grass blades (instanced)
     * @param time Current time for wind animation
     */
    void draw(float time) const;

    /**
     * @brief Check if grass is ready to render
     */
    [[nodiscard]] bool is_valid() const { return m_vao != 0; }

    /**
     * @brief Get number of grass blades
     */
    [[nodiscard]] u32 blade_count() const { return static_cast<u32>(m_instances.size()); }

    /**
     * @brief Get configuration
     */
    [[nodiscard]] const GrassConfig& config() const { return m_config; }

private:
    void create_blade_mesh();
    void upload_instances();

    GrassConfig m_config;
    std::vector<GrassInstance> m_instances;

    // OpenGL buffers
    u32 m_vao{0};
    u32 m_vbo{0};          // Blade quad vertices
    u32 m_instance_vbo{0}; // Instance data
};

} // namespace hz
