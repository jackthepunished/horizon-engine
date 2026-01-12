#pragma once

/**
 * @file hitbox_system.hpp
 * @brief Hitbox/Hurtbox system for FPS damage detection
 *
 * Provides precise hit detection for:
 * - Character body parts (head, body, limbs)
 * - Damage multipliers per body part
 * - Integration with Jolt Physics
 */

#include "engine/core/types.hpp"
#include "engine/physics/physics_world.hpp"

#include <string>
#include <vector>

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <entt/entt.hpp>
#include <glm/glm.hpp>

namespace hz {

/**
 * @brief Hitbox type for body part identification
 */
enum class HitboxType : u8 {
    Head,  // Critical damage zone
    Torso, // Main body
    LeftArm,
    RightArm,
    LeftLeg,
    RightLeg,
    Custom // User-defined
};

/**
 * @brief Shape type for hitbox colliders
 */
enum class HitboxShape : u8 { Sphere, Capsule, Box };

/**
 * @brief Single hitbox definition
 */
struct Hitbox {
    std::string name{"hitbox"};
    HitboxType type{HitboxType::Torso};
    HitboxShape shape{HitboxShape::Capsule};

    // Transform relative to entity
    glm::vec3 offset{0.0f};
    glm::vec3 rotation{0.0f}; // Euler angles

    // Shape dimensions
    glm::vec3 dimensions{0.5f}; // Box half-extents or [radius, height, 0] for capsule

    // Damage multiplier
    f32 damage_multiplier{1.0f};

    // Physics body (created at runtime)
    PhysicsBodyID body_id{};

    // Is this hitbox active?
    bool enabled{true};
};

/**
 * @brief Component containing all hitboxes for an entity
 */
struct HitboxComponent {
    std::vector<Hitbox> hitboxes;

    // Optional bone attachment names for skeletal meshes
    std::vector<std::string> bone_names;

    /**
     * @brief Get default FPS character hitboxes
     */
    static HitboxComponent create_humanoid();
};

/**
 * @brief Component for entities that can receive damage
 */
struct HurtboxComponent {
    f32 max_health{100.0f};
    f32 current_health{100.0f};
    f32 armor{0.0f};
    f32 max_armor{100.0f};

    // Damage reduction from armor (0.0 - 1.0)
    f32 armor_effectiveness{0.5f};

    // Invulnerability
    bool invulnerable{false};
    f32 invulnerability_timer{0.0f};

    // Death state
    bool is_dead{false};

    // Last damage info
    f32 last_damage_amount{0.0f};
    glm::vec3 last_damage_direction{0.0f};
    HitboxType last_hit_location{HitboxType::Torso};

    /**
     * @brief Apply damage to this entity
     * @param base_damage Base damage amount
     * @param hit_location Which hitbox was hit
     * @param damage_direction Direction damage came from
     * @param hitbox The specific hitbox that was hit (for multiplier)
     * @return Actual damage dealt
     */
    f32 apply_damage(f32 base_damage, HitboxType hit_location, const glm::vec3& damage_direction,
                     const Hitbox* hitbox = nullptr);

    /**
     * @brief Heal the entity
     * @param amount Amount to heal
     */
    void heal(f32 amount);

    /**
     * @brief Add armor
     * @param amount Amount of armor to add
     */
    void add_armor(f32 amount);
};

/**
 * @brief Damage event for event-driven damage system
 */
struct DamageEvent {
    entt::entity target{entt::null};
    entt::entity instigator{entt::null};
    f32 damage_amount{0.0f};
    f32 actual_damage{0.0f}; // After armor/multipliers
    HitboxType hit_location{HitboxType::Torso};
    glm::vec3 hit_point{0.0f};
    glm::vec3 hit_normal{0.0f};
    glm::vec3 damage_direction{0.0f};
};

/**
 * @brief Hitbox system for managing hitbox physics and queries
 */
class HitboxSystem {
public:
    HitboxSystem() = default;
    ~HitboxSystem() = default;

    /**
     * @brief Initialize the hitbox system
     * @param physics_world Reference to physics world
     */
    void init(PhysicsWorld& physics_world);

    /**
     * @brief Shutdown and cleanup
     */
    void shutdown();

    /**
     * @brief Update hitbox positions based on entity transforms
     * @param registry ECS registry
     */
    void update(entt::registry& registry);

    /**
     * @brief Create physics bodies for an entity's hitboxes
     * @param entity The entity
     * @param hitbox_comp The hitbox component
     * @param world_position Entity world position
     */
    void create_hitbox_bodies(entt::entity entity, HitboxComponent& hitbox_comp,
                              const glm::vec3& world_position);

    /**
     * @brief Destroy physics bodies for an entity's hitboxes
     * @param hitbox_comp The hitbox component
     */
    void destroy_hitbox_bodies(HitboxComponent& hitbox_comp);

    /**
     * @brief Perform a raycast and check for hitbox hits
     * @param origin Ray origin
     * @param direction Ray direction (normalized)
     * @param max_distance Maximum ray distance
     * @param registry ECS registry
     * @param out_hit Output hit information
     * @param out_hitbox Output hitbox that was hit
     * @param out_entity Output entity that was hit
     * @return true if a hitbox was hit
     */
    bool raycast_hitboxes(const glm::vec3& origin, const glm::vec3& direction, f32 max_distance,
                          entt::registry& registry, RaycastHit& out_hit, Hitbox*& out_hitbox,
                          entt::entity& out_entity);

private:
    PhysicsWorld* m_physics_world{nullptr};

    // Map from Jolt BodyID index to entity/hitbox for fast lookup
    std::unordered_map<u32, std::pair<entt::entity, size_t>> m_body_to_hitbox;
};

/**
 * @brief Get default damage multiplier for hitbox type
 */
inline f32 get_default_damage_multiplier(HitboxType type) {
    switch (type) {
    case HitboxType::Head:
        return 2.0f; // Headshot
    case HitboxType::Torso:
        return 1.0f; // Body shot
    case HitboxType::LeftArm:
    case HitboxType::RightArm:
        return 0.75f; // Arm shot
    case HitboxType::LeftLeg:
    case HitboxType::RightLeg:
        return 0.75f; // Leg shot
    default:
        return 1.0f;
    }
}

} // namespace hz
