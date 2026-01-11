#pragma once

/**
 * @file terrain.hpp
 * @brief Heightmap-based terrain with multi-texture splatting
 *
 * Generates terrain mesh from a heightmap image and supports
 * 4-texture blending via a splatmap (RGBA channels).
 */

#include "engine/core/types.hpp"
#include "mesh.hpp"

#include <memory>
#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace hz {

/**
 * @brief Terrain configuration
 */
struct TerrainConfig {
    float width{100.0f};        // World units in X
    float depth{100.0f};        // World units in Z
    float max_height{20.0f};    // Maximum height from heightmap
    float texture_scale{10.0f}; // UV tiling for detail textures
    u32 resolution{256};        // Vertices per side (if no heightmap)
};

/**
 * @brief Terrain vertex with extended data
 */
struct TerrainVertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texcoord;      // For detail textures (tiled)
    glm::vec2 splatcoord;    // For splatmap sampling (0-1)
};

/**
 * @brief Heightmap-based terrain mesh generator
 */
class Terrain {
public:
    Terrain() = default;
    ~Terrain() = default;

    HZ_NON_COPYABLE(Terrain);
    HZ_DEFAULT_MOVABLE(Terrain);

    /**
     * @brief Generate terrain from a heightmap image
     * @param heightmap_path Path to grayscale heightmap (PNG, JPG, etc.)
     * @param config Terrain configuration
     * @return true if successful
     */
    bool generate_from_heightmap(const std::string& heightmap_path, const TerrainConfig& config);

    /**
     * @brief Generate flat terrain (for testing)
     * @param config Terrain configuration
     */
    void generate_flat(const TerrainConfig& config);

    /**
     * @brief Generate terrain using Perlin noise
     * @param config Terrain configuration
     * @param seed Random seed
     * @param octaves Noise octaves (detail levels)
     * @param persistence Amplitude falloff per octave
     */
    void generate_procedural(const TerrainConfig& config, u32 seed = 0, 
                             u32 octaves = 4, float persistence = 0.5f);

    /**
     * @brief Draw the terrain
     */
    void draw() const;

    /**
     * @brief Get height at world position (for physics/gameplay)
     * @param x World X coordinate
     * @param z World Z coordinate
     * @return Interpolated height at position
     */
    [[nodiscard]] float get_height_at(float x, float z) const;

    /**
     * @brief Get terrain dimensions
     */
    [[nodiscard]] float width() const { return m_config.width; }
    [[nodiscard]] float depth() const { return m_config.depth; }
    [[nodiscard]] float max_height() const { return m_config.max_height; }

    /**
     * @brief Check if terrain is ready
     */
    [[nodiscard]] bool is_valid() const { return m_vao != 0; }

private:
    void calculate_normals(std::vector<TerrainVertex>& vertices, 
                          const std::vector<u32>& indices, u32 grid_width, u32 grid_depth);
    void upload_mesh(const std::vector<TerrainVertex>& vertices, 
                    const std::vector<u32>& indices);
    
    // Simple noise function for procedural generation
    static float noise2d(float x, float y, u32 seed);
    static float perlin2d(float x, float y, u32 seed, u32 octaves, float persistence);

    TerrainConfig m_config;
    std::vector<float> m_heightmap_data; // Cached heights for get_height_at()
    u32 m_heightmap_width{0};
    u32 m_heightmap_depth{0};

    // OpenGL buffers
    u32 m_vao{0};
    u32 m_vbo{0};
    u32 m_ebo{0};
    u32 m_index_count{0};
};

} // namespace hz
