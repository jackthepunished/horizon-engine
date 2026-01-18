/**
 * @file animation_system.cpp
 * @brief Implementation of skeletal animation system
 */

#include "animation_system.hpp"

#include <engine/core/log.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace game {

void AnimationSystem::init(hz::Model& character_model) {
    if (!character_model.has_skeleton()) {
        HZ_LOG_WARN("AnimationSystem: Character model has no skeleton");
        return;
    }

    // Setup IK chain for left arm: LeftArm(17) -> LeftForeArm(22) -> LeftHand(24)
    m_left_arm_chain.bone_ids = {17, 22, 24};
    m_left_arm_chain.calculate_length(*character_model.skeleton());
    m_ik_initialized = true;

    HZ_LOG_INFO("AnimationSystem initialized with IK chain");
}

void AnimationSystem::update(hz::Scene& scene, float dt) {
    auto view = scene.registry().view<hz::AnimatorComponent>();
    for (auto [entity, animator] : view.each()) {
        animator.update(dt);
    }
}

void AnimationSystem::apply_ik(hz::Scene& scene, hz::Model& character_model,
                               const glm::vec3& target_position) {
    if (!m_ik_enabled || !m_ik_initialized || !character_model.has_skeleton()) {
        return;
    }

    auto view =
        scene.registry().view<hz::TransformComponent, hz::MeshComponent, hz::AnimatorComponent>();

    for (auto [entity, tc, mc, ac] : view.each()) {
        // Find the character entity (model.index == 1)
        if (mc.mesh_type != hz::MeshComponent::MeshType::Model || mc.model.index != 1) {
            continue;
        }

        // Transform IK target from world space to model space
        glm::mat4 model_inv = glm::inverse(tc.get_transform());
        glm::vec3 local_target = glm::vec3(model_inv * glm::vec4(target_position, 1.0f));

        // Set pole vector (elbow direction - behind character)
        m_left_arm_ik.pole_vector = glm::vec3(0.0f, 0.0f, -50.0f);

        // Apply IK
        m_left_arm_ik.solve(*character_model.skeleton(), m_left_arm_chain, local_target,
                            ac.bone_transforms);
    }
}

void AnimationSystem::sync_with_player_movement(hz::Scene& scene, bool is_moving) {
    auto view = scene.registry().view<hz::AnimatorComponent, hz::MeshComponent>();

    for (auto [entity, animator, mesh] : view.each()) {
        // Only affect character model (index == 1)
        if (mesh.mesh_type != hz::MeshComponent::MeshType::Model || mesh.model.index != 1) {
            continue;
        }

        if (is_moving) {
            // Only start/resume animation if not already playing
            if (animator.state == hz::AnimationState::Paused) {
                animator.resume();
            } else if (animator.state == hz::AnimationState::Stopped) {
                // Only play from start when truly stopped
                if (animator.current_clip) {
                    animator.play(animator.current_clip, true);
                }
            }
            // If already Playing, let it continue
        } else {
            // Not moving - pause animation (preserves current_time)
            animator.pause();
        }
    }
}

} // namespace game
