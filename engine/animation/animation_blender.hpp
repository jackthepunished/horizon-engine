#pragma once

/**
 * @file animation_blender.hpp
 * @brief Animation blending utilities for smooth transitions and complex animation states
 */

#include "skeleton.hpp"

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace hz {

/**
 * @brief Simple cross-fade blend between two animations
 */
class AnimationCrossFade {
public:
    /**
     * @brief Blend two animation poses
     *
     * @param skeleton The skeleton structure
     * @param from Source animation
     * @param to Target animation
     * @param time_from Current time in source animation
     * @param time_to Current time in target animation
     * @param blend_factor 0.0 = full 'from', 1.0 = full 'to'
     * @param output Output bone transforms
     */
    void blend(const Skeleton& skeleton, const AnimationClip& from, const AnimationClip& to,
               float time_from, float time_to, float blend_factor, std::vector<glm::mat4>& output);

private:
    // Temporary storage for intermediate transforms
    std::vector<glm::mat4> m_from_transforms;
    std::vector<glm::mat4> m_to_transforms;
};

/**
 * @brief Blend tree node for parameter-driven animation blending
 */
struct BlendTreeNode {
    std::shared_ptr<AnimationClip> clip;
    float threshold{0.0f}; // Parameter threshold for this clip
    float current_time{0.0f};
};

/**
 * @brief 1D Blend Tree (e.g., blend between walk/run based on speed)
 */
class BlendTree1D {
public:
    /**
     * @brief Add a clip to the blend tree
     */
    void add_clip(std::shared_ptr<AnimationClip> clip, float threshold);

    /**
     * @brief Update the blend tree
     *
     * @param skeleton The skeleton
     * @param parameter Current parameter value (e.g., speed)
     * @param dt Delta time
     * @param output Output transforms
     */
    void update(const Skeleton& skeleton, float parameter, float dt,
                std::vector<glm::mat4>& output);

private:
    std::vector<BlendTreeNode> m_nodes;

    // Temp storage
    std::vector<glm::mat4> m_temp_transforms_a;
    std::vector<glm::mat4> m_temp_transforms_b;
};

/**
 * @brief Layered animation blending
 *
 * Allows blending a partial animation (e.g., upper body aiming)
 * on top of a base animation (e.g., walking).
 */
class LayeredBlend {
public:
    /**
     * @brief Blend overlay animation on top of base for specific bones
     *
     * @param skeleton The skeleton
     * @param base Base animation transforms
     * @param overlay Overlay animation clip
     * @param overlay_time Time in overlay animation
     * @param overlay_bones Bone IDs to apply overlay to (and their children)
     * @param weight Blend weight [0-1]
     * @param output Output transforms (can be same as base)
     */
    void blend(const Skeleton& skeleton, const std::vector<glm::mat4>& base,
               const AnimationClip& overlay, float overlay_time,
               const std::vector<i32>& overlay_bones, float weight, std::vector<glm::mat4>& output);

private:
    std::vector<glm::mat4> m_overlay_transforms;
    std::vector<bool> m_affected_bones; // Cache which bones are affected
};

/**
 * @brief Animation state for the state machine
 */
struct AnimationState {
    std::string name;
    std::shared_ptr<AnimationClip> clip;
    bool loop{true};
    float speed{1.0f};

    // Current playback state
    float current_time{0.0f};
};

/**
 * @brief Transition between animation states
 */
struct AnimationTransition {
    std::string from_state;
    std::string to_state;
    float duration{0.2f}; // Cross-fade duration

    // Optional condition (returns true when transition should trigger)
    std::function<bool()> condition;
};

/**
 * @brief Animation State Machine
 *
 * Manages animation states and transitions between them.
 */
class AnimationStateMachine {
public:
    /**
     * @brief Add a state to the machine
     */
    void add_state(const std::string& name, std::shared_ptr<AnimationClip> clip, bool loop = true,
                   float speed = 1.0f);

    /**
     * @brief Add a transition between states
     */
    void add_transition(const std::string& from, const std::string& to, float duration = 0.2f,
                        std::function<bool()> condition = nullptr);

    /**
     * @brief Set the current state (immediate, no transition)
     */
    void set_state(const std::string& name);

    /**
     * @brief Trigger a transition to another state
     */
    void transition_to(const std::string& name, float duration = -1.0f);

    /**
     * @brief Update the state machine
     */
    void update(const Skeleton& skeleton, float dt, std::vector<glm::mat4>& output);

    /**
     * @brief Get current state name
     */
    [[nodiscard]] const std::string& current_state_name() const { return m_current_state; }

    /**
     * @brief Check if currently transitioning
     */
    [[nodiscard]] bool is_transitioning() const { return m_transitioning; }

private:
    std::unordered_map<std::string, AnimationState> m_states;
    std::vector<AnimationTransition> m_transitions;

    std::string m_current_state;
    std::string m_next_state;

    bool m_transitioning{false};
    float m_transition_time{0.0f};
    float m_transition_duration{0.2f};

    AnimationCrossFade m_cross_fader;
};

} // namespace hz
