#pragma once

/**
 * @file animator.hpp
 * @brief Animator component for skeletal animation playback
 */

#include "skeleton.hpp"

#include <memory>

namespace hz {

/**
 * @brief Animation playback state
 */
enum class AnimationState { Stopped, Playing, Paused };

/**
 * @brief Component for controlling skeletal animation
 */
struct AnimatorComponent {
    std::shared_ptr<Skeleton> skeleton;
    std::shared_ptr<AnimationClip> current_clip;

    AnimationState state{AnimationState::Stopped};
    float current_time{0.0f};
    float playback_speed{1.0f};
    bool loop{true};

    // Cached bone transforms (updated each frame)
    std::vector<glm::mat4> bone_transforms;

    /**
     * @brief Start playing an animation
     */
    void play(std::shared_ptr<AnimationClip> clip, bool loop_animation = true) {
        current_clip = std::move(clip);
        current_time = 0.0f;
        loop = loop_animation;
        state = AnimationState::Playing;
    }

    /**
     * @brief Stop animation
     */
    void stop() {
        state = AnimationState::Stopped;
        current_time = 0.0f;
    }

    /**
     * @brief Pause animation
     */
    void pause() {
        if (state == AnimationState::Playing) {
            state = AnimationState::Paused;
        }
    }

    /**
     * @brief Resume animation
     */
    void resume() {
        if (state == AnimationState::Paused) {
            state = AnimationState::Playing;
        }
    }

    /**
     * @brief Update animation (call each frame)
     */
    void update(float delta_time) {
        if (state != AnimationState::Playing || !skeleton || !current_clip) {
            return;
        }

        current_time += delta_time * playback_speed * current_clip->ticks_per_second;

        if (current_time >= current_clip->duration) {
            if (loop) {
                current_time = fmod(current_time, current_clip->duration);
            } else {
                current_time = current_clip->duration;
                state = AnimationState::Stopped;
            }
        }

        // Calculate bone transforms
        skeleton->calculate_bone_transforms(*current_clip, current_time, bone_transforms);
    }

    /**
     * @brief Get current animation progress [0, 1]
     */
    [[nodiscard]] float get_progress() const {
        if (!current_clip || current_clip->duration <= 0.0f)
            return 0.0f;
        return current_time / current_clip->duration;
    }
};

} // namespace hz
