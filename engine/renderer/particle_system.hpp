#pragma once

/**
 * @file particle_system.hpp
 * @brief GPU-accelerated particle system with instanced rendering
 */

#include "engine/core/types.hpp"

#include <functional>
#include <random>
#include <vector>

#include <glm/glm.hpp>

namespace hz {

/**
 * @brief Single particle data
 */
struct Particle {
    glm::vec3 position{0.0f};
    glm::vec3 velocity{0.0f};
    glm::vec4 color{1.0f};        // RGBA
    glm::vec4 color_end{1.0f};    // Color to fade to
    float size{1.0f};
    float size_end{0.0f};         // Size to shrink/grow to
    float rotation{0.0f};
    float rotation_speed{0.0f};
    float life{1.0f};             // Remaining life (0-1)
    float max_life{1.0f};         // Initial life duration
    bool active{false};
};

/**
 * @brief GPU instance data (matches shader)
 */
struct ParticleInstanceData {
    glm::vec3 position;
    glm::vec4 color;
    float size;
    float rotation;
};

/**
 * @brief Particle emitter configuration
 */
struct ParticleEmitterConfig {
    // Emission settings
    glm::vec3 position{0.0f};           // Emitter world position
    glm::vec3 position_variance{0.0f};  // Random offset from position
    u32 max_particles{1000};            // Maximum particles in pool
    float emit_rate{50.0f};             // Particles per second
    bool burst_mode{false};             // Emit all at once
    
    // Velocity
    glm::vec3 velocity{0.0f, 1.0f, 0.0f};   // Initial velocity
    glm::vec3 velocity_variance{0.5f};       // Random velocity variation
    glm::vec3 gravity{0.0f, -9.8f, 0.0f};   // Gravity acceleration
    float drag{0.0f};                        // Air resistance
    
    // Appearance
    glm::vec4 color_start{1.0f, 1.0f, 1.0f, 1.0f}; // Starting color
    glm::vec4 color_end{1.0f, 1.0f, 1.0f, 0.0f};   // Ending color (fades)
    float size_start{1.0f};
    float size_end{0.0f};
    float rotation_speed{0.0f};
    float rotation_variance{0.0f};
    
    // Lifetime
    float life_min{1.0f};
    float life_max{2.0f};
    
    // Blend mode
    bool additive_blend{false};  // true for fire/glow effects
};

/**
 * @brief Particle emitter that spawns and manages particles
 */
class ParticleEmitter {
public:
    ParticleEmitter() = default;
    ~ParticleEmitter();

    HZ_NON_COPYABLE(ParticleEmitter);
    ParticleEmitter(ParticleEmitter&& other) noexcept;
    ParticleEmitter& operator=(ParticleEmitter&& other) noexcept;

    /**
     * @brief Initialize emitter
     */
    void init(const ParticleEmitterConfig& config);

    /**
     * @brief Update particles (physics, lifetime, etc.)
     * @param dt Delta time in seconds
     */
    void update(float dt);

    /**
     * @brief Draw all active particles
     * Must have particle shader bound and uniforms set
     */
    void draw() const;

    /**
     * @brief Emit a burst of particles
     * @param count Number of particles to emit
     */
    void emit_burst(u32 count);

    /**
     * @brief Set emitter position
     */
    void set_position(const glm::vec3& pos) { m_config.position = pos; }

    /**
     * @brief Get emitter position
     */
    [[nodiscard]] glm::vec3 position() const { return m_config.position; }

    /**
     * @brief Enable/disable continuous emission
     */
    void set_emitting(bool emit) { m_emitting = emit; }

    /**
     * @brief Check if emitting
     */
    [[nodiscard]] bool is_emitting() const { return m_emitting; }

    /**
     * @brief Get configuration
     */
    [[nodiscard]] const ParticleEmitterConfig& config() const { return m_config; }

    /**
     * @brief Update configuration
     */
    void set_config(const ParticleEmitterConfig& config) { m_config = config; }

    /**
     * @brief Get active particle count
     */
    [[nodiscard]] u32 active_count() const { return m_active_count; }

    /**
     * @brief Check if initialized
     */
    [[nodiscard]] bool is_valid() const { return m_vao != 0; }

private:
    void create_quad_mesh();
    void upload_instance_data();
    void emit_particle();
    
    [[nodiscard]] float random_range(float min, float max);
    [[nodiscard]] glm::vec3 random_vec3(const glm::vec3& variance);

    ParticleEmitterConfig m_config;
    std::vector<Particle> m_particles;
    std::vector<ParticleInstanceData> m_instance_data;
    
    bool m_emitting{true};
    float m_emit_accumulator{0.0f};
    u32 m_active_count{0};
    
    std::mt19937 m_rng;
    
    // OpenGL buffers
    u32 m_vao{0};
    u32 m_quad_vbo{0};     // Quad vertices
    u32 m_instance_vbo{0}; // Instance data
};

/**
 * @brief Manages multiple particle emitters
 */
class ParticleSystem {
public:
    ParticleSystem() = default;
    ~ParticleSystem() = default;

    /**
     * @brief Create a new emitter
     * @param config Emitter configuration
     * @return Emitter ID
     */
    u32 create_emitter(const ParticleEmitterConfig& config);

    /**
     * @brief Get emitter by ID
     */
    [[nodiscard]] ParticleEmitter* get_emitter(u32 id);

    /**
     * @brief Remove emitter
     */
    void remove_emitter(u32 id);

    /**
     * @brief Update all emitters
     */
    void update(float dt);

    /**
     * @brief Draw all emitters
     */
    void draw() const;

    /**
     * @brief Get number of emitters
     */
    [[nodiscard]] usize emitter_count() const { return m_emitters.size(); }

private:
    std::vector<std::unique_ptr<ParticleEmitter>> m_emitters;
};

// ============================================================================
// Preset configurations for common effects
// ============================================================================

namespace ParticlePresets {

/**
 * @brief Fire/flame effect
 */
inline ParticleEmitterConfig fire() {
    ParticleEmitterConfig cfg;
    cfg.max_particles = 500;
    cfg.emit_rate = 100.0f;
    cfg.velocity = glm::vec3(0.0f, 3.0f, 0.0f);
    cfg.velocity_variance = glm::vec3(0.5f, 1.0f, 0.5f);
    cfg.gravity = glm::vec3(0.0f, 2.0f, 0.0f); // Rises
    cfg.color_start = glm::vec4(1.0f, 0.8f, 0.2f, 1.0f); // Yellow-orange
    cfg.color_end = glm::vec4(1.0f, 0.2f, 0.0f, 0.0f);   // Red, fade out
    cfg.size_start = 0.5f;
    cfg.size_end = 0.1f;
    cfg.life_min = 0.5f;
    cfg.life_max = 1.5f;
    cfg.additive_blend = true;
    return cfg;
}

/**
 * @brief Smoke effect
 */
inline ParticleEmitterConfig smoke() {
    ParticleEmitterConfig cfg;
    cfg.max_particles = 300;
    cfg.emit_rate = 30.0f;
    cfg.velocity = glm::vec3(0.0f, 2.0f, 0.0f);
    cfg.velocity_variance = glm::vec3(1.0f, 0.5f, 1.0f);
    cfg.gravity = glm::vec3(0.0f, 1.0f, 0.0f); // Rises slowly
    cfg.drag = 0.5f;
    cfg.color_start = glm::vec4(0.3f, 0.3f, 0.3f, 0.8f);
    cfg.color_end = glm::vec4(0.5f, 0.5f, 0.5f, 0.0f);
    cfg.size_start = 0.3f;
    cfg.size_end = 2.0f; // Expands
    cfg.life_min = 2.0f;
    cfg.life_max = 4.0f;
    cfg.additive_blend = false;
    return cfg;
}

/**
 * @brief Sparkles/magic effect
 */
inline ParticleEmitterConfig sparkles() {
    ParticleEmitterConfig cfg;
    cfg.max_particles = 200;
    cfg.emit_rate = 50.0f;
    cfg.position_variance = glm::vec3(1.0f);
    cfg.velocity = glm::vec3(0.0f);
    cfg.velocity_variance = glm::vec3(2.0f);
    cfg.gravity = glm::vec3(0.0f, -2.0f, 0.0f);
    cfg.color_start = glm::vec4(1.0f, 1.0f, 0.5f, 1.0f); // Bright yellow
    cfg.color_end = glm::vec4(0.5f, 0.8f, 1.0f, 0.0f);   // Light blue
    cfg.size_start = 0.2f;
    cfg.size_end = 0.0f;
    cfg.rotation_speed = 5.0f;
    cfg.rotation_variance = 3.0f;
    cfg.life_min = 0.5f;
    cfg.life_max = 1.0f;
    cfg.additive_blend = true;
    return cfg;
}

/**
 * @brief Water splash effect
 */
inline ParticleEmitterConfig splash() {
    ParticleEmitterConfig cfg;
    cfg.max_particles = 100;
    cfg.emit_rate = 0.0f; // Burst only
    cfg.burst_mode = true;
    cfg.velocity = glm::vec3(0.0f, 5.0f, 0.0f);
    cfg.velocity_variance = glm::vec3(3.0f, 2.0f, 3.0f);
    cfg.gravity = glm::vec3(0.0f, -15.0f, 0.0f);
    cfg.color_start = glm::vec4(0.7f, 0.9f, 1.0f, 0.8f);
    cfg.color_end = glm::vec4(0.7f, 0.9f, 1.0f, 0.0f);
    cfg.size_start = 0.15f;
    cfg.size_end = 0.05f;
    cfg.life_min = 0.5f;
    cfg.life_max = 1.0f;
    cfg.additive_blend = false;
    return cfg;
}

/**
 * @brief Rain effect (falling particles)
 */
inline ParticleEmitterConfig rain() {
    ParticleEmitterConfig cfg;
    cfg.max_particles = 2000;
    cfg.emit_rate = 500.0f;
    cfg.position_variance = glm::vec3(50.0f, 0.0f, 50.0f);
    cfg.velocity = glm::vec3(0.0f, -15.0f, 0.0f);
    cfg.velocity_variance = glm::vec3(0.5f, 2.0f, 0.5f);
    cfg.gravity = glm::vec3(0.0f, -5.0f, 0.0f);
    cfg.color_start = glm::vec4(0.6f, 0.7f, 0.9f, 0.6f);
    cfg.color_end = glm::vec4(0.6f, 0.7f, 0.9f, 0.3f);
    cfg.size_start = 0.1f;
    cfg.size_end = 0.1f;
    cfg.life_min = 1.0f;
    cfg.life_max = 2.0f;
    cfg.additive_blend = false;
    return cfg;
}

} // namespace ParticlePresets

} // namespace hz
