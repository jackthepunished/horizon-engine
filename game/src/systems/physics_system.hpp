#pragma once

/**
 * @file physics_system.hpp
 * @brief System for managing physics body creation and synchronization
 */

#include <engine/physics/physics_world.hpp>
#include <engine/scene/components.hpp>
#include <engine/scene/scene.hpp>

namespace game {

/**
 * @brief Manages physics body creation and ECS synchronization
 */
class PhysicsSystem {
public:
    /**
     * @brief Initialize the physics system
     * @param physics Physics world reference
     */
    void init(hz::PhysicsWorld& physics);

    /**
     * @brief Update physics simulation and sync with ECS
     * @param scene Game scene
     * @param physics Physics world
     * @param dt Delta time in seconds
     */
    void update(hz::Scene& scene, hz::PhysicsWorld& physics, float dt);

private:
    /**
     * @brief Create physics bodies for entities that need them
     */
    void create_physics_bodies(hz::Scene& scene, hz::PhysicsWorld& physics);

    /**
     * @brief Sync physics state back to ECS transforms
     */
    void sync_physics_to_ecs(hz::Scene& scene, hz::PhysicsWorld& physics);
};

} // namespace game
