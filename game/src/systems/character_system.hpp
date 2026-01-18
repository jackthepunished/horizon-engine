#pragma once

/**
 * @file character_system.hpp
 * @brief System for syncing first-person character arms with camera
 */

#include "game_config.hpp"

#include <engine/scene/components.hpp>
#include <engine/scene/scene.hpp>
#include <glm/glm.hpp>

namespace game {

/**
 * @brief Syncs first-person character model with camera position/rotation
 */
class CharacterSystem {
public:
    /**
     * @brief Update character position to follow camera
     * @param scene Game scene
     * @param camera_position Camera world position
     * @param camera_rotation Camera rotation (pitch, yaw, roll)
     */
    void update(hz::Scene& scene, const glm::vec3& camera_position,
                const glm::vec3& camera_rotation);
};

} // namespace game
