#pragma once

/**
 * @file fps_character_controller.hpp
 * @brief FPS-specific character controller using Jolt CharacterVirtual
 *
 * Provides smooth, responsive first-person movement with:
 * - Capsule-based collision
 * - Slope handling and step climbing
 * - Ground detection via raycasting
 * - Crouch/sprint/jump mechanics
 */

#include "engine/core/types.hpp"
#include "engine/physics/physics_world.hpp"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <glm/glm.hpp>

namespace hz {

/**
 * @brief Character controller state
 */
enum class CharacterState : u8 { Idle, Walking, Sprinting, Crouching, Jumping, Falling };

/**
 * @brief Character controller configuration
 */
struct CharacterControllerConfig {
    // Capsule dimensions
    f32 standing_height = 1.8f;
    f32 crouching_height = 1.0f;
    f32 capsule_radius = 0.3f;

    // Movement speeds (m/s)
    f32 walk_speed = 4.0f;
    f32 sprint_speed = 7.0f;
    f32 crouch_speed = 2.0f;
    f32 air_control = 0.3f; // Movement control while airborne

    // Jump & gravity
    f32 jump_force = 8.0f;
    f32 gravity = 20.0f;

    // Ground detection
    f32 ground_check_distance = 0.1f;
    f32 max_slope_angle = 45.0f; // degrees
    f32 step_height = 0.35f;

    // Physics
    f32 skin_width = 0.08f;
    f32 ground_friction = 6.0f;
    f32 air_friction = 0.0f;
};

/**
 * @brief FPS Character Controller
 *
 * Handles first-person player movement with proper collision response.
 * Uses Jolt's CharacterVirtual for maximum responsiveness.
 */
class FPSCharacterController {
public:
    FPSCharacterController() = default;
    ~FPSCharacterController();

    HZ_NON_COPYABLE(FPSCharacterController);
    HZ_NON_MOVABLE(FPSCharacterController);

    /**
     * @brief Initialize the character controller
     * @param physics_world Reference to the physics world
     * @param position Initial position
     * @param config Controller configuration
     * @return true if initialization successful
     */
    bool init(PhysicsWorld& physics_world, const glm::vec3& position,
              const CharacterControllerConfig& config = {});

    /**
     * @brief Shutdown and cleanup
     */
    void shutdown();

    /**
     * @brief Update controller physics (call every fixed timestep)
     * @param delta_time Time since last update in seconds
     */
    void update(f32 delta_time);

    // =========================================================================
    // Movement Input
    // =========================================================================

    /**
     * @brief Set movement input direction
     * @param direction Normalized input direction (world space XZ plane)
     */
    void set_move_input(const glm::vec3& direction);

    /**
     * @brief Apply a look rotation to determine movement direction
     * @param yaw_radians Character yaw in radians (for relative movement)
     */
    void set_look_direction(f32 yaw_radians);

    /**
     * @brief Request a jump (only works if grounded)
     */
    void jump();

    /**
     * @brief Set sprint state
     * @param sprinting true to sprint
     */
    void set_sprinting(bool sprinting);

    /**
     * @brief Set crouch state
     * @param crouching true to crouch
     */
    void set_crouching(bool crouching);

    // =========================================================================
    // State Queries
    // =========================================================================

    [[nodiscard]] bool is_grounded() const { return m_is_grounded; }
    [[nodiscard]] bool is_crouching() const { return m_is_crouching; }
    [[nodiscard]] bool is_sprinting() const { return m_is_sprinting; }
    [[nodiscard]] CharacterState get_state() const { return m_current_state; }
    [[nodiscard]] bool can_stand_up() const;

    // =========================================================================
    // Transform
    // =========================================================================

    [[nodiscard]] glm::vec3 get_position() const { return m_position; }
    [[nodiscard]] glm::vec3 get_velocity() const { return m_velocity; }
    [[nodiscard]] glm::vec3 get_ground_normal() const { return m_ground_normal; }
    [[nodiscard]] f32 get_current_height() const;

    /**
     * @brief Get the eye position (camera position)
     * @return Position at eye level
     */
    [[nodiscard]] glm::vec3 get_eye_position() const;

    /**
     * @brief Teleport the character to a new position
     * @param position New world position
     */
    void set_position(const glm::vec3& position);

    // =========================================================================
    // Configuration
    // =========================================================================

    [[nodiscard]] const CharacterControllerConfig& get_config() const { return m_config; }
    void set_config(const CharacterControllerConfig& config);

private:
    void update_ground_state();
    void update_velocity(f32 delta_time);
    void update_position(f32 delta_time);
    void update_character_height(f32 delta_time);
    CharacterState determine_state() const;

    // Convert between Jolt and GLM
    static JPH::Vec3 to_jolt(const glm::vec3& v) { return JPH::Vec3(v.x, v.y, v.z); }
    static glm::vec3 to_glm(const JPH::Vec3& v) { return glm::vec3(v.GetX(), v.GetY(), v.GetZ()); }

private:
    PhysicsWorld* m_physics_world{nullptr};
    JPH::Ref<JPH::CharacterVirtual> m_character;
    CharacterControllerConfig m_config;

    // Position & velocity
    glm::vec3 m_position{0.0f};
    glm::vec3 m_velocity{0.0f};
    glm::vec3 m_move_input{0.0f};
    f32 m_look_yaw{0.0f};

    // Ground state
    glm::vec3 m_ground_normal{0.0f, 1.0f, 0.0f};
    bool m_is_grounded{false};

    // Input state
    bool m_jump_requested{false};
    bool m_is_sprinting{false};
    bool m_is_crouching{false};
    bool m_wants_to_crouch{false};

    // Current character height (for smooth crouch transitions)
    f32 m_current_height{1.8f};
    f32 m_target_height{1.8f};

    // State
    CharacterState m_current_state{CharacterState::Idle};
    bool m_initialized{false};
};

/**
 * @brief CharacterControllerComponent for ECS integration
 */
struct CharacterControllerComponent {
    std::unique_ptr<FPSCharacterController> controller;
    CharacterControllerConfig config;

    // Input state (set by input system)
    glm::vec3 move_input{0.0f};
    f32 look_yaw{0.0f};
    bool jump{false};
    bool sprint{false};
    bool crouch{false};
};

} // namespace hz
