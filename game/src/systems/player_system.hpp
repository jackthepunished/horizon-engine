#pragma once

/**
 * @file player_system.hpp
 * @brief FPS player movement and input handling system
 */

#include "game_config.hpp"

#include <engine/physics/physics_world.hpp>
#include <engine/platform/input.hpp>
#include <engine/platform/window.hpp>
#include <engine/scene/components.hpp>
#include <engine/scene/scene.hpp>
#include <glm/glm.hpp>

namespace game {

/**
 * @brief Player state for FPS movement
 */
struct PlayerState {
    float vertical_velocity{0.0f};
    bool is_grounded{true};
    bool is_crouching{false};
    bool is_sprinting{false};
    bool is_moving{false};

    float movement_speed{GameConfig::MOVEMENT_SPEED};
    float mouse_sensitivity{GameConfig::MOUSE_SENSITIVITY};
};

/**
 * @brief Handles player input, movement, and physics
 */
class PlayerSystem {
public:
    PlayerSystem() = default;
    ~PlayerSystem() = default;

    /**
     * @brief Initialize the player system
     * @param scene Game scene
     * @param physics Physics world
     */
    void init(hz::Scene& scene, hz::PhysicsWorld& physics);

    /**
     * @brief Update player state based on input
     * @param scene Game scene
     * @param input Input manager
     * @param window Window for cursor state
     * @param dt Delta time
     */
    void update(hz::Scene& scene, hz::InputManager& input, hz::Window& window, float dt);

    /**
     * @brief Handle shooting input
     * @param scene Game scene
     * @param input Input manager
     * @param physics Physics world
     */
    void handle_shooting(hz::Scene& scene, hz::InputManager& input, hz::PhysicsWorld& physics);

    /**
     * @brief Get current player state (for animation sync, etc.)
     */
    [[nodiscard]] const PlayerState& state() const { return m_state; }

    /**
     * @brief Get player entity
     */
    [[nodiscard]] hz::Entity player_entity() const { return m_player_entity; }

    /**
     * @brief Get camera position from primary camera
     */
    [[nodiscard]] glm::vec3 get_camera_position(hz::Scene& scene) const;

    /**
     * @brief Get camera rotation from primary camera
     */
    [[nodiscard]] glm::vec3 get_camera_rotation(hz::Scene& scene) const;

    /**
     * @brief Get camera front vector (look direction)
     */
    [[nodiscard]] glm::vec3 get_front_vector(hz::Scene& scene) const;

private:
    void handle_mouse_look(hz::TransformComponent& tc, hz::InputManager& input, hz::Window& window,
                           float dt);
    void handle_movement(hz::TransformComponent& tc, hz::InputManager& input, float dt);
    void handle_jumping_and_crouching(hz::TransformComponent& tc, hz::InputManager& input,
                                      float dt);
    void apply_gravity(hz::TransformComponent& tc, float dt);
    void create_impact_vfx(hz::Scene& scene, const glm::vec3& position);

    PlayerState m_state;
    hz::Entity m_player_entity{entt::null};
};

} // namespace game
