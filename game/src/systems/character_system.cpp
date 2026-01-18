/**
 * @file character_system.cpp
 * @brief Implementation of first-person character synchronization
 */

#include "character_system.hpp"

#include <glm/gtc/matrix_transform.hpp>

namespace game {

void CharacterSystem::update(hz::Scene& scene, const glm::vec3& camera_position,
                             const glm::vec3& camera_rotation) {
    // Find character entity (model.index == 1)
    auto view = scene.registry().view<hz::TransformComponent, hz::MeshComponent>();

    for (auto [entity, tc, mc] : view.each()) {
        if (mc.mesh_type != hz::MeshComponent::MeshType::Model || mc.model.index != 1) {
            continue;
        }

        // Calculate horizontal front direction (ignore pitch for character facing)
        float yaw_rad = glm::radians(camera_rotation.y);
        glm::vec3 forward_flat(cos(yaw_rad), 0.0f, sin(yaw_rad));
        forward_flat = glm::normalize(forward_flat);

        // Position character at camera position, offset down and forward
        tc.position = camera_position;
        tc.position.y -= GameConfig::CHARACTER_EYE_OFFSET;
        tc.position += forward_flat * GameConfig::CHARACTER_FORWARD_OFFSET;

        // Character faces camera direction (rotated 180 degrees)
        tc.rotation.y = camera_rotation.y + 180.0f;
        tc.scale = glm::vec3(1.0f);

        break; // Only one character
    }
}

} // namespace game
