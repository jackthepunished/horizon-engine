#pragma once

/**
 * @file animation_system.hpp
 * @brief System for updating skeletal animations
 */

#include <engine/animation/animator.hpp>
#include <engine/animation/ik_solver.hpp>
#include <engine/assets/model.hpp>
#include <engine/scene/components.hpp>
#include <engine/scene/scene.hpp>
#include <glm/glm.hpp>

namespace game {

/**
 * @brief Manages skeletal animation updates and IK solving
 */
class AnimationSystem {
public:
    /**
     * @brief Initialize the animation system
     * @param character_model Reference to the character model for IK setup
     */
    void init(hz::Model& character_model);

    /**
     * @brief Update all animations
     * @param scene Game scene
     * @param dt Delta time in seconds
     */
    void update(hz::Scene& scene, float dt);

    /**
     * @brief Apply IK to character arms
     * @param scene Game scene
     * @param character_model Character model with skeleton
     * @param target_position World-space IK target position
     */
    void apply_ik(hz::Scene& scene, hz::Model& character_model, const glm::vec3& target_position);

    /**
     * @brief Sync character animation state with player movement
     * @param scene Game scene
     * @param is_moving Whether the player is currently moving
     */
    void sync_with_player_movement(hz::Scene& scene, bool is_moving);

    /**
     * @brief Enable or disable IK
     */
    void set_ik_enabled(bool enabled) { m_ik_enabled = enabled; }
    [[nodiscard]] bool is_ik_enabled() const { return m_ik_enabled; }

private:
    bool m_ik_enabled{false};
    bool m_ik_initialized{false};

    hz::TwoBoneIK m_left_arm_ik;
    hz::IKChain m_left_arm_chain;
};

} // namespace game
