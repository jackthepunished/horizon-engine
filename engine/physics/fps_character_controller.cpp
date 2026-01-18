// Include physics config first (sets up JPH_DEBUG_RENDERER before Jolt headers)
#include "fps_character_controller.hpp"

#include "engine/core/log.hpp"
#include "physics_config.hpp"

#include <cmath>

#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <glm/gtc/constants.hpp>

namespace hz {

FPSCharacterController::~FPSCharacterController() {
    shutdown();
}

bool FPSCharacterController::init(PhysicsWorld& physics_world, const glm::vec3& position,
                                  const CharacterControllerConfig& config) {
    if (m_initialized) {
        HZ_ENGINE_WARN("Character controller already initialized");
        return false;
    }

    m_physics_world = &physics_world;
    m_config = config;
    m_position = position;
    m_current_height = config.standing_height;
    m_target_height = config.standing_height;

    // Create the capsule shape
    // Jolt capsule is defined by half-height of the cylinder part + radius
    f32 half_height = (m_config.standing_height - 2.0f * m_config.capsule_radius) * 0.5f;
    half_height = std::max(half_height, 0.01f); // Ensure positive

    JPH::CapsuleShapeSettings capsule_settings(half_height, m_config.capsule_radius);
    JPH::ShapeSettings::ShapeResult shape_result = capsule_settings.Create();

    if (shape_result.HasError()) {
        HZ_ENGINE_ERROR("Failed to create capsule shape: {}", shape_result.GetError().c_str());
        return false;
    }

    // Create rotated translated shape to position capsule correctly
    // The capsule is created with its axis along Y, which is what we want
    JPH::RotatedTranslatedShapeSettings rtshape_settings(
        JPH::Vec3(0, m_config.standing_height * 0.5f, 0), // Center the capsule
        JPH::Quat::sIdentity(), shape_result.Get());

    JPH::ShapeSettings::ShapeResult final_shape_result = rtshape_settings.Create();
    if (final_shape_result.HasError()) {
        HZ_ENGINE_ERROR("Failed to create rotated translated shape: {}",
                        final_shape_result.GetError().c_str());
        return false;
    }

    // Create character settings
    JPH::CharacterVirtualSettings character_settings;
    character_settings.mMaxSlopeAngle = glm::radians(m_config.max_slope_angle);
    character_settings.mMaxStrength = 100.0f;
    character_settings.mShape = final_shape_result.Get();
    character_settings.mBackFaceMode = JPH::EBackFaceMode::CollideWithBackFaces;
    character_settings.mCharacterPadding = m_config.skin_width;
    character_settings.mPenetrationRecoverySpeed = 1.0f;
    character_settings.mPredictiveContactDistance = 0.1f;

    // Create the character (JPH::Ref handles reference counting automatically)
    m_character = new JPH::CharacterVirtual(&character_settings,
                                            JPH::RVec3(position.x, position.y, position.z),
                                            JPH::Quat::sIdentity(), m_physics_world->jolt_system());
    // Note: JPH::Ref<T> is Jolt's intrusive smart pointer - it automatically
    // decrements the reference count when destroyed/reassigned to nullptr

    m_initialized = true;
    HZ_ENGINE_INFO("FPS Character Controller initialized at ({:.2f}, {:.2f}, {:.2f})", position.x,
                   position.y, position.z);
    return true;
}

void FPSCharacterController::shutdown() {
    if (!m_initialized)
        return;

    m_character = nullptr;
    m_physics_world = nullptr;
    m_initialized = false;

    HZ_ENGINE_INFO("FPS Character Controller shutdown");
}

void FPSCharacterController::update(f32 delta_time) {
    if (!m_initialized || !m_character)
        return;

    // Update ground state first
    update_ground_state();

    // Update velocity based on input and physics
    update_velocity(delta_time);

    // Update character height (crouch transitions)
    update_character_height(delta_time);

    // Apply movement
    update_position(delta_time);

    // Update state
    m_current_state = determine_state();
}

void FPSCharacterController::set_move_input(const glm::vec3& direction) {
    // Normalize input if length > 1
    f32 length = glm::length(direction);
    if (length > 1.0f) {
        m_move_input = direction / length;
    } else {
        m_move_input = direction;
    }
}

void FPSCharacterController::set_look_direction(f32 yaw_radians) {
    m_look_yaw = yaw_radians;
}

void FPSCharacterController::jump() {
    if (m_is_grounded && !m_jump_requested) {
        m_jump_requested = true;
    }
}

void FPSCharacterController::set_sprinting(bool sprinting) {
    // Can't sprint while crouching
    m_is_sprinting = sprinting && !m_is_crouching;
}

void FPSCharacterController::set_crouching(bool crouching) {
    m_wants_to_crouch = crouching;

    // Immediate crouch down
    if (crouching) {
        m_is_crouching = true;
        m_is_sprinting = false;
    }
    // Standing up requires space check
    else if (!crouching && m_is_crouching) {
        if (can_stand_up()) {
            m_is_crouching = false;
        }
    }
}

bool FPSCharacterController::can_stand_up() const {
    if (!m_initialized || !m_physics_world)
        return true;

    // Raycast upward to check for obstacles
    f32 height_diff = m_config.standing_height - m_config.crouching_height;
    glm::vec3 origin = m_position + glm::vec3(0.0f, m_config.crouching_height - 0.1f, 0.0f);
    glm::vec3 direction(0.0f, 1.0f, 0.0f);

    RaycastHit hit = m_physics_world->raycast(origin, direction, height_diff + 0.1f);
    return !hit.hit;
}

f32 FPSCharacterController::get_current_height() const {
    return m_current_height;
}

glm::vec3 FPSCharacterController::get_eye_position() const {
    // Eye position is at 90% of current height
    f32 eye_height = m_current_height * 0.9f;
    return m_position + glm::vec3(0.0f, eye_height, 0.0f);
}

void FPSCharacterController::set_position(const glm::vec3& position) {
    m_position = position;
    if (m_character) {
        m_character->SetPosition(JPH::RVec3(position.x, position.y, position.z));
    }
}

void FPSCharacterController::set_config(const CharacterControllerConfig& config) {
    m_config = config;
    // Note: Changing height while initialized would require recreating the shape
}

void FPSCharacterController::update_ground_state() {
    if (!m_character)
        return;

    JPH::CharacterVirtual::EGroundState ground_state = m_character->GetGroundState();
    m_is_grounded = (ground_state == JPH::CharacterVirtual::EGroundState::OnGround);

    if (m_is_grounded) {
        JPH::Vec3 normal = m_character->GetGroundNormal();
        m_ground_normal = to_glm(normal);
    } else {
        m_ground_normal = glm::vec3(0.0f, 1.0f, 0.0f);
    }
}

void FPSCharacterController::update_velocity(f32 delta_time) {
    // Determine target speed based on state
    f32 target_speed = m_config.walk_speed;
    if (m_is_crouching) {
        target_speed = m_config.crouch_speed;
    } else if (m_is_sprinting) {
        target_speed = m_config.sprint_speed;
    }

    // Convert input to world-space movement direction based on yaw
    glm::vec3 forward(std::sin(m_look_yaw), 0.0f, std::cos(m_look_yaw));
    glm::vec3 right(std::cos(m_look_yaw), 0.0f, -std::sin(m_look_yaw));

    glm::vec3 move_direction = forward * m_move_input.z + right * m_move_input.x;
    if (glm::length(move_direction) > 0.01f) {
        move_direction = glm::normalize(move_direction);
    }

    // Calculate target horizontal velocity
    glm::vec3 target_velocity = move_direction * target_speed;

    // Apply friction/acceleration
    f32 control = m_is_grounded ? 1.0f : m_config.air_control;
    f32 friction = m_is_grounded ? m_config.ground_friction : m_config.air_friction;

    // Horizontal velocity interpolation
    glm::vec2 current_horizontal(m_velocity.x, m_velocity.z);
    glm::vec2 target_horizontal(target_velocity.x, target_velocity.z);

    if (m_is_grounded) {
        // Ground movement - accelerate towards target
        glm::vec2 diff = target_horizontal - current_horizontal;
        f32 acceleration = friction * delta_time * control;
        if (glm::length(diff) > acceleration) {
            diff = glm::normalize(diff) * acceleration;
        }
        current_horizontal += diff;
    } else {
        // Air movement - limited control
        glm::vec2 air_input =
            glm::vec2(move_direction.x, move_direction.z) * target_speed * control;
        current_horizontal += air_input * delta_time;

        // Cap air speed
        f32 air_speed = glm::length(current_horizontal);
        if (air_speed > target_speed) {
            current_horizontal = (current_horizontal / air_speed) * target_speed;
        }
    }

    m_velocity.x = current_horizontal.x;
    m_velocity.z = current_horizontal.y;

    // Vertical velocity
    if (m_is_grounded) {
        // Apply slight downward velocity to maintain ground contact
        m_velocity.y = -0.1f;

        // Handle jump
        if (m_jump_requested) {
            m_velocity.y = m_config.jump_force;
            m_jump_requested = false;
            m_is_grounded = false;
        }
    } else {
        // Apply gravity
        m_velocity.y -= m_config.gravity * delta_time;
    }
}

void FPSCharacterController::update_position(f32 delta_time) {
    if (!m_character || !m_physics_world)
        return;

    // Apply velocity to character
    m_character->SetLinearVelocity(to_jolt(m_velocity));

    // Create update settings
    JPH::CharacterVirtual::ExtendedUpdateSettings update_settings;
    update_settings.mStickToFloorStepDown = JPH::Vec3(0, -m_config.step_height, 0);
    update_settings.mWalkStairsStepUp = JPH::Vec3(0, m_config.step_height, 0);
    update_settings.mWalkStairsMinStepForward = 0.02f;
    update_settings.mWalkStairsStepForwardTest = 0.15f;
    update_settings.mWalkStairsCosAngleForwardContact = std::cos(glm::radians(75.0f));

    // Get physics system interfaces
    JPH::PhysicsSystem* physics_system = m_physics_world->jolt_system();

    // Create a temp allocator for this update (1MB should be plenty for character update)
    JPH::TempAllocatorImpl temp_allocator(1024 * 1024);

    // Update character
    JPH::Vec3 gravity(0, -m_config.gravity, 0);
    m_character->ExtendedUpdate(
        delta_time, gravity, update_settings,
        physics_system->GetDefaultBroadPhaseLayerFilter(PhysicsLayers::MOVING),
        physics_system->GetDefaultLayerFilter(PhysicsLayers::MOVING), {}, {}, temp_allocator);

    // Get new position
    JPH::RVec3 new_pos = m_character->GetPosition();
    m_position = glm::vec3(new_pos.GetX(), new_pos.GetY(), new_pos.GetZ());

    // Update velocity from character (for collision responses)
    JPH::Vec3 new_vel = m_character->GetLinearVelocity();
    m_velocity = to_glm(new_vel);
}

void FPSCharacterController::update_character_height(f32 delta_time) {
    // Update target height based on crouch state
    m_target_height = m_is_crouching ? m_config.crouching_height : m_config.standing_height;

    // Smooth height transition
    f32 height_speed = 8.0f; // Units per second
    if (std::abs(m_current_height - m_target_height) > 0.01f) {
        f32 height_delta = height_speed * delta_time;
        if (m_current_height < m_target_height) {
            m_current_height = std::min(m_current_height + height_delta, m_target_height);
        } else {
            m_current_height = std::max(m_current_height - height_delta, m_target_height);
        }

        // TODO: Recreate character shape with new height for proper collision
        // This is expensive, so we might want to just adjust the position instead
    }
}

CharacterState FPSCharacterController::determine_state() const {
    if (!m_is_grounded) {
        if (m_velocity.y > 0.1f) {
            return CharacterState::Jumping;
        }
        return CharacterState::Falling;
    }

    f32 horizontal_speed = std::sqrt(m_velocity.x * m_velocity.x + m_velocity.z * m_velocity.z);

    if (horizontal_speed < 0.1f) {
        return CharacterState::Idle;
    }

    if (m_is_crouching) {
        return CharacterState::Crouching;
    }

    if (m_is_sprinting) {
        return CharacterState::Sprinting;
    }

    return CharacterState::Walking;
}

} // namespace hz
