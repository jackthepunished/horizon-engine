#pragma once

/**
 * @file cinematic_camera.hpp
 * @brief Cinematic camera system for cutscenes and scripted sequences
 */

#include "engine/core/types.hpp"

#include <functional>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace hz {

/**
 * @brief Camera movement types for cinematic shots
 */
enum class CameraMoveType {
    Cut,       // Instant transition
    Lerp,      // Linear interpolation
    EaseIn,    // Slow start
    EaseOut,   // Slow end
    EaseInOut, // Slow start and end
    Dolly,     // Track along path
    Orbit      // Orbit around target
};

/**
 * @brief A single camera keyframe
 */
struct CameraKeyframe {
    glm::vec3 position{0.0f};
    glm::vec3 target{0.0f, 0.0f, -1.0f}; // Look-at target
    float fov{45.0f};
    float duration{1.0f}; // Time to reach this keyframe
    CameraMoveType move_type{CameraMoveType::EaseInOut};
};

/**
 * @brief Cinematic camera controller
 *
 * Supports keyframe animation, look-at constraints, letterbox mode,
 * and camera shake effects.
 */
class CinematicCamera {
public:
    CinematicCamera() = default;

    // ========================================================================
    // Keyframe Animation
    // ========================================================================

    /**
     * @brief Add a keyframe to the sequence
     */
    void add_keyframe(const CameraKeyframe& keyframe);

    /**
     * @brief Clear all keyframes
     */
    void clear_keyframes();

    /**
     * @brief Start playing the keyframe sequence
     */
    void play();

    /**
     * @brief Pause playback
     */
    void pause();

    /**
     * @brief Stop and reset to beginning
     */
    void stop();

    /**
     * @brief Update the camera (call every frame when active)
     */
    void update(float dt);

    // ========================================================================
    // Camera Properties
    // ========================================================================

    [[nodiscard]] const glm::vec3& position() const { return m_current_position; }
    [[nodiscard]] const glm::vec3& target() const { return m_current_target; }
    [[nodiscard]] float fov() const { return m_current_fov; }

    /**
     * @brief Get view matrix for rendering
     */
    [[nodiscard]] glm::mat4 view_matrix() const;

    /**
     * @brief Get projection matrix
     */
    [[nodiscard]] glm::mat4 projection_matrix(float aspect_ratio) const;

    // ========================================================================
    // Letterbox Mode
    // ========================================================================

    void set_letterbox(bool enabled, float ratio = 2.39f) {
        m_letterbox_enabled = enabled;
        m_letterbox_ratio = ratio;
    }

    [[nodiscard]] bool letterbox_enabled() const { return m_letterbox_enabled; }
    [[nodiscard]] float letterbox_ratio() const { return m_letterbox_ratio; }

    /**
     * @brief Get letterbox bar height (0-1 of screen height, per bar)
     */
    [[nodiscard]] float letterbox_bar_height(float screen_aspect) const;

    // ========================================================================
    // Camera Shake
    // ========================================================================

    /**
     * @brief Trigger camera shake
     * @param intensity Shake intensity
     * @param duration Shake duration
     * @param frequency Shake frequency
     */
    void shake(float intensity, float duration, float frequency = 20.0f);

    // ========================================================================
    // Callbacks
    // ========================================================================

    void set_on_complete(std::function<void()> callback) { m_on_complete = std::move(callback); }

    // ========================================================================
    // State
    // ========================================================================

    [[nodiscard]] bool is_playing() const { return m_playing; }
    [[nodiscard]] bool is_complete() const { return m_complete; }
    [[nodiscard]] u32 current_keyframe_index() const { return m_current_keyframe; }

private:
    // Keyframes
    std::vector<CameraKeyframe> m_keyframes;
    u32 m_current_keyframe{0};
    float m_keyframe_time{0.0f};

    // Current interpolated state
    glm::vec3 m_current_position{0.0f};
    glm::vec3 m_current_target{0.0f, 0.0f, -1.0f};
    float m_current_fov{45.0f};

    // Playback state
    bool m_playing{false};
    bool m_complete{false};

    // Letterbox
    bool m_letterbox_enabled{false};
    float m_letterbox_ratio{2.39f}; // Cinemascope

    // Shake
    float m_shake_intensity{0.0f};
    float m_shake_duration{0.0f};
    float m_shake_time{0.0f};
    float m_shake_frequency{20.0f};
    glm::vec3 m_shake_offset{0.0f};

    // Callbacks
    std::function<void()> m_on_complete;

    // Helpers
    [[nodiscard]] float apply_easing(float t, CameraMoveType type) const;
    void update_shake(float dt);
};

} // namespace hz
