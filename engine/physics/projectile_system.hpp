#pragma once

/**
 * @file projectile_system.hpp
 * @brief Projectile physics for FPS weapons
 *
 * Supports three projectile types:
 * - Hitscan: Instant raycast (pistols, rifles)
 * - Ballistic: Physics-based projectiles (grenades, rockets)
 * - Continuous: Fast-moving projectiles with CCD (sniper, machine gun)
 */

#include "engine/core/types.hpp"
#include "engine/physics/hitbox_system.hpp"
#include "engine/physics/physics_world.hpp"

#include <functional>

#include <entt/entt.hpp>
#include <glm/glm.hpp>

namespace hz {

/**
 * @brief Type of projectile
 */
enum class ProjectileType : u8 {
    Hitscan,   // Instant raycast, no travel time
    Ballistic, // Physics-based, affected by gravity
    Continuous // Fast projectile with CCD (swept collision)
};

/**
 * @brief Projectile data definition (template)
 */
struct ProjectileData {
    std::string name{"bullet"};
    ProjectileType type{ProjectileType::Hitscan};

    // Damage
    f32 base_damage{25.0f};
    f32 damage_falloff_start{20.0f}; // Distance where falloff starts
    f32 damage_falloff_end{50.0f};   // Distance where min damage is reached
    f32 min_damage_multiplier{0.5f}; // Minimum damage at max range

    // Ballistic properties
    f32 muzzle_velocity{400.0f}; // m/s for ballistic projectiles
    f32 gravity_scale{1.0f};     // 1.0 = normal gravity
    f32 drag_coefficient{0.0f};  // Air resistance

    // Lifetime
    f32 max_lifetime{10.0f}; // Seconds before despawn
    f32 max_range{1000.0f};  // Max distance for hitscan

    // Penetration
    f32 penetration_power{0.0f}; // 0 = no penetration
    u8 max_penetrations{0};      // Max surfaces to penetrate

    // Explosion (for rockets, grenades)
    bool explosive{false};
    f32 explosion_radius{0.0f};
    f32 explosion_damage{0.0f};
    f32 explosion_falloff{1.0f}; // Damage falloff from center

    // Visual (for spawning effects)
    bool has_tracer{true};
    f32 tracer_width{0.02f};
    glm::vec3 tracer_color{1.0f, 0.9f, 0.7f};
};

/**
 * @brief Active projectile instance
 */
struct ProjectileComponent {
    ProjectileData data;

    // State
    glm::vec3 position{0.0f};
    glm::vec3 velocity{0.0f};
    glm::vec3 start_position{0.0f};
    f32 time_alive{0.0f};
    f32 distance_traveled{0.0f};

    // Owner
    entt::entity owner{entt::null};

    // Penetration tracking
    u8 penetration_count{0};

    // Physics body (for ballistic)
    PhysicsBodyID body_id{};

    // Flags
    bool pending_destroy{false};
};

/**
 * @brief Result of a hitscan shot
 */
struct HitscanResult {
    bool hit{false};
    glm::vec3 hit_point{0.0f};
    glm::vec3 hit_normal{0.0f};
    f32 distance{0.0f};

    // Entity hit info
    entt::entity hit_entity{entt::null};
    Hitbox* hit_hitbox{nullptr};
    HitboxType hit_location{HitboxType::Torso};

    // Damage info
    f32 raw_damage{0.0f};
    f32 final_damage{0.0f};
};

/**
 * @brief Callback for projectile events
 */
using ProjectileHitCallback = std::function<void(const HitscanResult&)>;
using ProjectileExplosionCallback = std::function<void(const glm::vec3&, f32, f32)>;

/**
 * @brief Projectile system for managing all projectile physics
 */
class ProjectileSystem {
public:
    ProjectileSystem() = default;
    ~ProjectileSystem() = default;

    /**
     * @brief Initialize the projectile system
     * @param physics_world Reference to physics world
     * @param hitbox_system Reference to hitbox system
     */
    void init(PhysicsWorld& physics_world, HitboxSystem& hitbox_system);

    /**
     * @brief Shutdown and cleanup
     */
    void shutdown();

    /**
     * @brief Update all projectiles
     * @param registry ECS registry
     * @param delta_time Time since last frame
     */
    void update(entt::registry& registry, f32 delta_time);

    /**
     * @brief Fire a hitscan weapon (instant raycast)
     * @param origin Fire origin (muzzle position)
     * @param direction Fire direction (normalized)
     * @param data Projectile data
     * @param owner Entity that fired
     * @param registry ECS registry
     * @return Hitscan result
     */
    HitscanResult fire_hitscan(const glm::vec3& origin, const glm::vec3& direction,
                               const ProjectileData& data, entt::entity owner,
                               entt::registry& registry);

    /**
     * @brief Spawn a ballistic projectile
     * @param origin Spawn position
     * @param direction Fire direction (normalized)
     * @param data Projectile data
     * @param owner Entity that fired
     * @param registry ECS registry
     * @return Created projectile entity
     */
    entt::entity spawn_ballistic(const glm::vec3& origin, const glm::vec3& direction,
                                 const ProjectileData& data, entt::entity owner,
                                 entt::registry& registry);

    /**
     * @brief Set hit callback
     */
    void set_hit_callback(ProjectileHitCallback callback) { m_hit_callback = std::move(callback); }

    /**
     * @brief Set explosion callback
     */
    void set_explosion_callback(ProjectileExplosionCallback callback) {
        m_explosion_callback = std::move(callback);
    }

    /**
     * @brief Calculate damage falloff based on distance
     */
    static f32 calculate_damage_falloff(const ProjectileData& data, f32 distance);

private:
    void update_ballistic_projectiles(entt::registry& registry, f32 delta_time);
    void process_projectile_hit(entt::registry& registry, ProjectileComponent& proj,
                                const RaycastHit& hit, Hitbox* hitbox, entt::entity hit_entity);
    void process_explosion(entt::registry& registry, const glm::vec3& position,
                           const ProjectileData& data);
    void cleanup_destroyed_projectiles(entt::registry& registry);

    PhysicsWorld* m_physics_world{nullptr};
    HitboxSystem* m_hitbox_system{nullptr};

    ProjectileHitCallback m_hit_callback;
    ProjectileExplosionCallback m_explosion_callback;

    // Gravity constant
    static constexpr f32 GRAVITY = 9.81f;
};

// ============================================================================
// Predefined Projectile Templates
// ============================================================================

namespace ProjectileTemplates {

inline ProjectileData pistol_bullet() {
    ProjectileData data;
    data.name = "9mm";
    data.type = ProjectileType::Hitscan;
    data.base_damage = 25.0f;
    data.damage_falloff_start = 15.0f;
    data.damage_falloff_end = 40.0f;
    data.min_damage_multiplier = 0.6f;
    data.max_range = 100.0f;
    return data;
}

inline ProjectileData rifle_bullet() {
    ProjectileData data;
    data.name = "5.56mm";
    data.type = ProjectileType::Hitscan;
    data.base_damage = 35.0f;
    data.damage_falloff_start = 30.0f;
    data.damage_falloff_end = 80.0f;
    data.min_damage_multiplier = 0.5f;
    data.max_range = 200.0f;
    data.penetration_power = 0.3f;
    data.max_penetrations = 1;
    return data;
}

inline ProjectileData sniper_bullet() {
    ProjectileData data;
    data.name = "7.62mm";
    data.type = ProjectileType::Hitscan;
    data.base_damage = 100.0f;
    data.damage_falloff_start = 100.0f;
    data.damage_falloff_end = 300.0f;
    data.min_damage_multiplier = 0.8f;
    data.max_range = 500.0f;
    data.penetration_power = 0.8f;
    data.max_penetrations = 2;
    return data;
}

inline ProjectileData shotgun_pellet() {
    ProjectileData data;
    data.name = "12gauge_pellet";
    data.type = ProjectileType::Hitscan;
    data.base_damage = 15.0f;
    data.damage_falloff_start = 5.0f;
    data.damage_falloff_end = 20.0f;
    data.min_damage_multiplier = 0.2f;
    data.max_range = 30.0f;
    return data;
}

inline ProjectileData rocket() {
    ProjectileData data;
    data.name = "rocket";
    data.type = ProjectileType::Ballistic;
    data.base_damage = 50.0f; // Direct hit
    data.muzzle_velocity = 30.0f;
    data.gravity_scale = 0.1f;
    data.max_lifetime = 10.0f;
    data.explosive = true;
    data.explosion_radius = 5.0f;
    data.explosion_damage = 120.0f;
    data.explosion_falloff = 0.5f;
    data.has_tracer = true;
    data.tracer_color = glm::vec3(1.0f, 0.5f, 0.0f);
    return data;
}

inline ProjectileData grenade() {
    ProjectileData data;
    data.name = "frag_grenade";
    data.type = ProjectileType::Ballistic;
    data.base_damage = 10.0f;
    data.muzzle_velocity = 15.0f;
    data.gravity_scale = 1.0f;
    data.max_lifetime = 3.0f; // Fuse time
    data.explosive = true;
    data.explosion_radius = 8.0f;
    data.explosion_damage = 150.0f;
    data.explosion_falloff = 0.3f;
    return data;
}

} // namespace ProjectileTemplates

} // namespace hz
