#pragma once

/**
 * @file physics_interactions.hpp
 * @brief Physics interaction systems for FPS gameplay
 *
 * Includes:
 * - Destructible objects with health and debris
 * - Interactive physics props (push, grab, throw)
 * - Bullet penetration through materials
 */

#include "engine/core/types.hpp"
#include "engine/physics/physics_world.hpp"
#include "engine/physics/projectile_system.hpp"

#include <functional>
#include <string>
#include <vector>

#include <entt/entt.hpp>
#include <glm/glm.hpp>

namespace hz {

// ============================================================================
// Material System for Physics Interactions
// ============================================================================

/**
 * @brief Physical material type for penetration and destruction
 */
enum class PhysicsMaterialType : u8 {
    Default,
    Wood,
    Metal,
    Glass,
    Concrete,
    Flesh,
    Plastic,
    Water
};

/**
 * @brief Physical material properties
 */
struct PhysicsMaterial {
    PhysicsMaterialType type{PhysicsMaterialType::Default};
    std::string name{"default"};

    // Penetration properties
    f32 penetration_resistance{1.0f}; // How hard to penetrate (0 = easy, 1 = hard)
    f32 damage_reduction{0.3f};       // Damage reduced per penetration
    f32 thickness{0.1f};              // Thickness of material in meters

    // Destruction properties
    f32 hardness{1.0f}; // Resistance to damage
    bool is_destructible{false};

    // Sound properties (for footsteps, impacts)
    std::string impact_sound;
    std::string footstep_sound;

    // Visual effects
    std::string impact_particle;
    std::string destroy_particle;
};

/**
 * @brief Default material definitions
 */
namespace Materials {

inline PhysicsMaterial wood() {
    PhysicsMaterial m;
    m.type = PhysicsMaterialType::Wood;
    m.name = "wood";
    m.penetration_resistance = 0.3f;
    m.damage_reduction = 0.2f;
    m.thickness = 0.05f;
    m.hardness = 0.5f;
    m.is_destructible = true;
    m.impact_sound = "impact_wood";
    m.footstep_sound = "footstep_wood";
    return m;
}

inline PhysicsMaterial metal() {
    PhysicsMaterial m;
    m.type = PhysicsMaterialType::Metal;
    m.name = "metal";
    m.penetration_resistance = 0.9f;
    m.damage_reduction = 0.6f;
    m.thickness = 0.02f;
    m.hardness = 2.0f;
    m.is_destructible = false;
    m.impact_sound = "impact_metal";
    m.footstep_sound = "footstep_metal";
    return m;
}

inline PhysicsMaterial glass() {
    PhysicsMaterial m;
    m.type = PhysicsMaterialType::Glass;
    m.name = "glass";
    m.penetration_resistance = 0.1f;
    m.damage_reduction = 0.05f;
    m.thickness = 0.01f;
    m.hardness = 0.1f;
    m.is_destructible = true;
    m.impact_sound = "impact_glass";
    return m;
}

inline PhysicsMaterial concrete() {
    PhysicsMaterial m;
    m.type = PhysicsMaterialType::Concrete;
    m.name = "concrete";
    m.penetration_resistance = 0.95f;
    m.damage_reduction = 0.7f;
    m.thickness = 0.15f;
    m.hardness = 3.0f;
    m.is_destructible = false;
    m.impact_sound = "impact_concrete";
    m.footstep_sound = "footstep_concrete";
    return m;
}

inline PhysicsMaterial flesh() {
    PhysicsMaterial m;
    m.type = PhysicsMaterialType::Flesh;
    m.name = "flesh";
    m.penetration_resistance = 0.2f;
    m.damage_reduction = 0.1f;
    m.thickness = 0.3f;
    m.hardness = 0.3f;
    m.is_destructible = false;
    m.impact_sound = "impact_flesh";
    return m;
}

} // namespace Materials

// ============================================================================
// Destructible Objects
// ============================================================================

/**
 * @brief Destruction stage for progressive destruction
 */
struct DestructionStage {
    f32 health_threshold;   // Health % to trigger this stage
    std::string model_path; // Model to switch to at this stage
    std::string sound;      // Sound to play
    std::string particle;   // Particle effect
};

/**
 * @brief Component for destructible objects
 */
struct DestructibleComponent {
    f32 max_health{100.0f};
    f32 current_health{100.0f};
    PhysicsMaterial material;

    // Destruction stages (progressive damage)
    std::vector<DestructionStage> stages;
    int current_stage{0};

    // Debris spawning on destruction
    bool spawn_debris{true};
    std::string debris_model;
    int debris_count{5};
    f32 debris_force{10.0f};

    // Events
    bool is_destroyed{false};
    glm::vec3 last_hit_point{0.0f};
    glm::vec3 last_hit_direction{0.0f};

    /**
     * @brief Apply damage to the destructible
     * @param damage Damage amount
     * @param hit_point World position of hit
     * @param hit_direction Direction damage came from
     * @return true if destroyed
     */
    bool apply_damage(f32 damage, const glm::vec3& hit_point, const glm::vec3& hit_direction);
};

// ============================================================================
// Interactive Physics Props
// ============================================================================

/**
 * @brief Interaction type for physics props
 */
enum class InteractionType : u8 {
    None,
    Push,     // Can be pushed by player/physics
    Grab,     // Can be picked up and held
    Throw,    // Can be thrown (requires Grab)
    Activate, // Button, lever, etc.
    Carry     // Heavy object, slow movement while carrying
};

/**
 * @brief Component for interactive physics objects
 */
struct PhysicsPropComponent {
    InteractionType interaction_type{InteractionType::Push};
    PhysicsMaterial material;

    // Physics properties
    f32 mass{10.0f};
    f32 friction{0.5f};
    f32 restitution{0.3f}; // Bounciness

    // Interaction properties
    f32 push_force_multiplier{1.0f};
    f32 throw_force{15.0f};
    f32 grab_distance{1.5f};

    // Damage on collision
    bool deals_collision_damage{false};
    f32 min_damage_velocity{5.0f};
    f32 damage_per_velocity{2.0f};

    // State
    bool is_grabbed{false};
    entt::entity grabbed_by{entt::null};

    // Constraints
    bool lock_rotation{false};
    glm::vec3 allowed_movement{1.0f, 1.0f, 1.0f}; // Per-axis movement multiplier
};

/**
 * @brief Component for objects that are currently being held
 */
struct GrabbedObjectComponent {
    entt::entity grabber{entt::null};
    f32 grab_distance{1.5f};
    glm::vec3 grab_offset{0.0f};
    f32 hold_spring{100.0f};
    f32 hold_damping{10.0f};
};

// ============================================================================
// Bullet Penetration System
// ============================================================================

/**
 * @brief Result of a penetration check
 */
struct PenetrationResult {
    bool can_penetrate{false};
    f32 remaining_damage{0.0f};
    f32 exit_distance{0.0f};
    glm::vec3 exit_point{0.0f};
    glm::vec3 exit_direction{0.0f};
    PhysicsMaterial material;
};

/**
 * @brief Bullet penetration calculator
 */
class BulletPenetration {
public:
    /**
     * @brief Check if a bullet can penetrate a surface
     * @param projectile Projectile data
     * @param hit Hit information
     * @param material Material of surface hit
     * @param current_damage Current bullet damage
     * @return Penetration result
     */
    static PenetrationResult check_penetration(const ProjectileData& projectile,
                                               const RaycastHit& hit,
                                               const PhysicsMaterial& material, f32 current_damage);

    /**
     * @brief Calculate damage after penetration
     */
    static f32 calculate_exit_damage(f32 entry_damage, const PhysicsMaterial& material);

    /**
     * @brief Calculate bullet deviation after penetration
     */
    static glm::vec3 calculate_exit_direction(const glm::vec3& entry_direction,
                                              const glm::vec3& surface_normal,
                                              const PhysicsMaterial& material);
};

// ============================================================================
// Physics Interaction System
// ============================================================================

/**
 * @brief Callbacks for physics events
 */
using DestructionCallback = std::function<void(entt::entity, const glm::vec3&)>;
using GrabCallback = std::function<void(entt::entity grabber, entt::entity prop)>;
using ThrowCallback = std::function<void(entt::entity prop, const glm::vec3& velocity)>;

/**
 * @brief System managing all physics interactions
 */
class PhysicsInteractionSystem {
public:
    PhysicsInteractionSystem() = default;
    ~PhysicsInteractionSystem() = default;

    void init(PhysicsWorld& physics_world);
    void shutdown();
    void update(entt::registry& registry, f32 delta_time);

    // =========================================================================
    // Destructibles
    // =========================================================================

    /**
     * @brief Apply damage to a destructible entity
     */
    void damage_destructible(entt::registry& registry, entt::entity entity, f32 damage,
                             const glm::vec3& hit_point, const glm::vec3& hit_direction);

    /**
     * @brief Destroy an entity and spawn debris
     */
    void destroy_object(entt::registry& registry, entt::entity entity);

    // =========================================================================
    // Grabbable Props
    // =========================================================================

    /**
     * @brief Try to grab a prop
     * @return true if grab successful
     */
    bool try_grab(entt::registry& registry, entt::entity grabber, entt::entity prop);

    /**
     * @brief Release a grabbed prop
     */
    void release_grab(entt::registry& registry, entt::entity grabber);

    /**
     * @brief Throw a grabbed prop
     */
    void throw_prop(entt::registry& registry, entt::entity grabber, const glm::vec3& direction);

    /**
     * @brief Update grabbed object position
     */
    void update_grab_position(entt::registry& registry, entt::entity grabber,
                              const glm::vec3& target_position);

    // =========================================================================
    // Bullet Penetration
    // =========================================================================

    /**
     * @brief Process bullet hit with penetration
     * @return list of all hit results including penetrations
     */
    std::vector<HitscanResult> process_bullet_with_penetration(entt::registry& registry,
                                                               const glm::vec3& origin,
                                                               const glm::vec3& direction,
                                                               const ProjectileData& projectile,
                                                               entt::entity shooter);

    // =========================================================================
    // Callbacks
    // =========================================================================

    void set_destruction_callback(DestructionCallback cb) {
        m_destruction_callback = std::move(cb);
    }
    void set_grab_callback(GrabCallback cb) { m_grab_callback = std::move(cb); }
    void set_throw_callback(ThrowCallback cb) { m_throw_callback = std::move(cb); }

private:
    void update_grabbed_objects(entt::registry& registry, f32 delta_time);
    void check_destruction_stages(entt::registry& registry, DestructibleComponent& destructible,
                                  entt::entity entity);
    void spawn_debris(entt::registry& registry, const glm::vec3& position,
                      const DestructibleComponent& destructible);

    PhysicsWorld* m_physics_world{nullptr};

    DestructionCallback m_destruction_callback;
    GrabCallback m_grab_callback;
    ThrowCallback m_throw_callback;
};

// ============================================================================
// Material Component (for entities)
// ============================================================================

/**
 * @brief Component to assign a material to an entity
 */
struct MaterialComponent {
    PhysicsMaterial material;
};

} // namespace hz
