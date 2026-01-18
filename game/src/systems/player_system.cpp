/**
 * @file player_system.cpp
 * @brief FPS player movement and input handling implementation
 */

#include "player_system.hpp"

#include <engine/core/log.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace game {

void PlayerSystem::init(hz::Scene& scene, hz::PhysicsWorld& physics) {
    HZ_UNUSED(physics);

    // Create player camera entity
    m_player_entity = scene.create_entity();

    auto& tc = scene.registry().emplace<hz::TransformComponent>(m_player_entity);
    tc.position = glm::vec3(0.0f, GameConfig::GROUND_LEVEL, 5.0f);
    tc.rotation = glm::vec3(0.0f, -90.0f, 0.0f); // Face -Z direction

    auto& cc = scene.registry().emplace<hz::CameraComponent>(m_player_entity);
    cc.primary = true;
    cc.fov = 60.0f;
    cc.near_plane = 0.1f;
    cc.far_plane = 1000.0f;

    HZ_LOG_INFO("PlayerSystem initialized");
}

void PlayerSystem::update(hz::Scene& scene, hz::InputManager& input, hz::Window& window, float dt) {
    // Find primary camera (the player)
    auto view = scene.registry().view<hz::TransformComponent, hz::CameraComponent>();
    for (auto [entity, tc, cc] : view.each()) {
        if (!cc.primary) {
            continue;
        }

        handle_mouse_look(tc, input, window, dt);
        handle_movement(tc, input, dt);
        handle_jumping_and_crouching(tc, input, dt);
        apply_gravity(tc, dt);

        // Only process primary camera
        break;
    }
}

void PlayerSystem::handle_mouse_look(hz::TransformComponent& tc, hz::InputManager& input,
                                     hz::Window& window, [[maybe_unused]] float dt) {
    if (!window.is_cursor_captured()) {
        return;
    }

    const auto& mouse = input.mouse();
    float x_offset = static_cast<float>(mouse.delta_x) * m_state.mouse_sensitivity;
    // Reversed since y-coordinates go from bottom to top
    float y_offset = static_cast<float>(-mouse.delta_y) * m_state.mouse_sensitivity;

    tc.rotation.y += x_offset; // Yaw
    tc.rotation.x += y_offset; // Pitch

    // Constrain pitch to avoid gimbal lock
    constexpr float MAX_PITCH = 89.0f;
    if (tc.rotation.x > MAX_PITCH) {
        tc.rotation.x = MAX_PITCH;
    }
    if (tc.rotation.x < -MAX_PITCH) {
        tc.rotation.x = -MAX_PITCH;
    }
}

void PlayerSystem::handle_movement(hz::TransformComponent& tc, hz::InputManager& input, float dt) {
    // Calculate front vector from rotation (ignoring pitch for XZ movement)
    glm::vec3 front;
    front.x = cos(glm::radians(tc.rotation.y)) * cos(glm::radians(tc.rotation.x));
    front.y = sin(glm::radians(tc.rotation.x));
    front.z = sin(glm::radians(tc.rotation.y)) * cos(glm::radians(tc.rotation.x));
    front = glm::normalize(front);

    glm::vec3 right = glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f)));

    // Calculate effective speed with modifiers
    float effective_speed = m_state.movement_speed;
    if (m_state.is_sprinting) {
        effective_speed *= GameConfig::SPRINT_MULTIPLIER;
    }
    if (m_state.is_crouching) {
        effective_speed *= GameConfig::CROUCH_MULTIPLIER;
    }

    // Track movement for animation
    m_state.is_moving = false;

    if (input.is_action_active(hz::InputManager::ACTION_MOVE_FORWARD)) {
        tc.position += front * effective_speed * dt;
        m_state.is_moving = true;
    }
    if (input.is_action_active(hz::InputManager::ACTION_MOVE_BACKWARD)) {
        tc.position -= front * effective_speed * dt;
        m_state.is_moving = true;
    }
    if (input.is_action_active(hz::InputManager::ACTION_MOVE_LEFT)) {
        tc.position -= right * effective_speed * dt;
        m_state.is_moving = true;
    }
    if (input.is_action_active(hz::InputManager::ACTION_MOVE_RIGHT)) {
        tc.position += right * effective_speed * dt;
        m_state.is_moving = true;
    }

    // Sprint toggle (using shift key or similar)
    // Note: You may want to add ACTION_SPRINT to InputManager
    // m_state.is_sprinting = input.is_action_active(hz::InputManager::ACTION_SPRINT);
}

void PlayerSystem::handle_jumping_and_crouching(hz::TransformComponent& tc, hz::InputManager& input,
                                                [[maybe_unused]] float dt) {
    HZ_UNUSED(tc);

    // Jump (only when grounded)
    if (input.is_action_just_pressed(hz::InputManager::ACTION_JUMP) && m_state.is_grounded) {
        m_state.vertical_velocity = GameConfig::JUMP_FORCE;
        m_state.is_grounded = false;
    }

    // Crouch toggle
    if (input.is_action_just_pressed(hz::InputManager::ACTION_CROUCH)) {
        m_state.is_crouching = !m_state.is_crouching;
    }
}

void PlayerSystem::apply_gravity(hz::TransformComponent& tc, float dt) {
    m_state.vertical_velocity += GameConfig::GRAVITY * dt;
    tc.position.y += m_state.vertical_velocity * dt;

    // Ground check
    float target_height =
        m_state.is_crouching ? GameConfig::CROUCH_HEIGHT : GameConfig::GROUND_LEVEL;
    if (tc.position.y <= target_height) {
        tc.position.y = target_height;
        m_state.vertical_velocity = 0.0f;
        m_state.is_grounded = true;
    }
}

void PlayerSystem::handle_shooting(hz::Scene& scene, hz::InputManager& input,
                                   hz::PhysicsWorld& physics) {
    if (!input.is_action_just_pressed(hz::InputManager::ACTION_PRIMARY_FIRE)) {
        return;
    }

    // Find primary camera for raycast origin
    auto cam_view = scene.registry().view<hz::TransformComponent, hz::CameraComponent>();
    for (auto [entity, tc, cc] : cam_view.each()) {
        if (!cc.primary) {
            continue;
        }

        // Calculate front vector
        glm::vec3 front;
        front.x = cos(glm::radians(tc.rotation.y)) * cos(glm::radians(tc.rotation.x));
        front.y = sin(glm::radians(tc.rotation.x));
        front.z = sin(glm::radians(tc.rotation.y)) * cos(glm::radians(tc.rotation.x));
        front = glm::normalize(front);

        // Perform raycast
        hz::RaycastHit hit = physics.raycast(tc.position, front, GameConfig::RAYCAST_MAX_DISTANCE);

        if (hit.hit) {
            HZ_LOG_INFO("Raycast Hit! BodyID: {}", hit.body_id.id.GetIndexAndSequenceNumber());

            // Apply impulse to hit object
            physics.apply_impulse(hit.body_id, front * GameConfig::IMPULSE_STRENGTH);

            // Create impact VFX entity
            create_impact_vfx(scene, hit.position);
        }

        break; // Only process primary camera
    }
}

void PlayerSystem::create_impact_vfx(hz::Scene& scene, const glm::vec3& position) {
    auto impact = scene.create_entity();

    auto& tc = scene.registry().emplace<hz::TransformComponent>(impact);
    tc.position = position;
    tc.scale = glm::vec3(GameConfig::IMPACT_VFX_SIZE);

    auto& mc = scene.registry().emplace<hz::MeshComponent>(impact);
    mc.primitive_name = "sphere";
    mc.albedo_color = glm::vec3(1.0f, 0.2f, 0.2f); // Red
    mc.metallic = 0.0f;
    mc.roughness = 0.8f;

    scene.registry().emplace<hz::LifetimeComponent>(impact).time_remaining =
        GameConfig::IMPACT_VFX_LIFETIME;
}

glm::vec3 PlayerSystem::get_camera_position(hz::Scene& scene) const {
    auto view = scene.registry().view<hz::TransformComponent, hz::CameraComponent>();
    for (auto [entity, tc, cc] : view.each()) {
        if (cc.primary) {
            return tc.position;
        }
    }
    return glm::vec3(0.0f);
}

glm::vec3 PlayerSystem::get_camera_rotation(hz::Scene& scene) const {
    auto view = scene.registry().view<hz::TransformComponent, hz::CameraComponent>();
    for (auto [entity, tc, cc] : view.each()) {
        if (cc.primary) {
            return tc.rotation;
        }
    }
    return glm::vec3(0.0f);
}

glm::vec3 PlayerSystem::get_front_vector(hz::Scene& scene) const {
    glm::vec3 rotation = get_camera_rotation(scene);
    glm::vec3 front;
    front.x = cos(glm::radians(rotation.y)) * cos(glm::radians(rotation.x));
    front.y = sin(glm::radians(rotation.x));
    front.z = sin(glm::radians(rotation.y)) * cos(glm::radians(rotation.x));
    return glm::normalize(front);
}

} // namespace game
