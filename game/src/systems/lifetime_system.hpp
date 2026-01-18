#pragma once

/**
 * @file lifetime_system.hpp
 * @brief System for managing entity lifetimes (VFX cleanup, etc.)
 */

#include <engine/scene/components.hpp>
#include <engine/scene/scene.hpp>

namespace game {

/**
 * @brief Manages entity lifetimes - destroys entities when their lifetime expires
 */
class LifetimeSystem {
public:
    /**
     * @brief Update all entities with LifetimeComponent
     * @param scene Game scene
     * @param dt Delta time in seconds
     */
    void update(hz::Scene& scene, float dt);
};

} // namespace game
