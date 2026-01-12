#pragma once

/**
 * @file billboard.hpp
 * @brief Billboard rendering system for vegetation, particles, etc.
 */

#include "engine/core/types.hpp"

#include <glm/glm.hpp>
#include <vector>

namespace hz {

struct BillboardInstance {
    glm::vec3 position{0.0f};
    glm::vec2 size{1.0f, 1.0f};  // Width, Height
    glm::vec4 color{1.0f};       // RGBA tint
};

struct BillboardConfig {
    u32 max_instances = 1000;
};

/**
 * @brief Instanced billboard renderer
 * 
 * Renders camera-facing quads efficiently using instancing.
 * Great for trees, bushes, grass, particles, etc.
 */
class Billboard {
public:
    explicit Billboard(const BillboardConfig& config = {});
    ~Billboard() noexcept;

    HZ_NON_COPYABLE(Billboard);

    // Move semantics
    Billboard(Billboard&& other) noexcept;
    Billboard& operator=(Billboard&& other) noexcept;

    /**
     * @brief Set all billboard instances
     */
    void set_instances(const std::vector<BillboardInstance>& instances);

    /**
     * @brief Add a single instance
     */
    void add_instance(const BillboardInstance& instance);

    /**
     * @brief Clear all instances
     */
    void clear();

    /**
     * @brief Update GPU buffer with current instances
     */
    void upload();

    /**
     * @brief Draw all billboards
     */
    void draw() const;

    [[nodiscard]] u32 instance_count() const { return static_cast<u32>(m_instances.size()); }
    [[nodiscard]] const BillboardConfig& config() const { return m_config; }

private:
    void init_quad();

    BillboardConfig m_config;
    std::vector<BillboardInstance> m_instances;

    u32 m_vao{0};
    u32 m_quad_vbo{0};
    u32 m_instance_vbo{0};
    bool m_dirty{true};
};

} // namespace hz
