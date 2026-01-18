#include "cinematic_camera.hpp"

#include <algorithm>
#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

namespace hz {

// ============================================================================
// Keyframe Animation
// ============================================================================

void CinematicCamera::add_keyframe(const CameraKeyframe& keyframe) {
    m_keyframes.push_back(keyframe);
}

void CinematicCamera::clear_keyframes() {
    m_keyframes.clear();
    m_current_keyframe = 0;
    m_keyframe_time = 0.0f;
    m_complete = false;
}

void CinematicCamera::play() {
    if (m_keyframes.empty())
        return;

    m_playing = true;
    m_complete = false;
    m_current_keyframe = 0;
    m_keyframe_time = 0.0f;

    // Set initial position
    m_current_position = m_keyframes[0].position;
    m_current_target = m_keyframes[0].target;
    m_current_fov = m_keyframes[0].fov;
}

void CinematicCamera::pause() {
    m_playing = false;
}

void CinematicCamera::stop() {
    m_playing = false;
    m_complete = false;
    m_current_keyframe = 0;
    m_keyframe_time = 0.0f;
}

void CinematicCamera::update(float dt) {
    // Update shake
    update_shake(dt);

    if (!m_playing || m_keyframes.empty())
        return;

    // Need at least 2 keyframes for interpolation
    if (m_keyframes.size() < 2) {
        m_current_position = m_keyframes[0].position;
        m_current_target = m_keyframes[0].target;
        m_current_fov = m_keyframes[0].fov;
        return;
    }

    // Get current and next keyframe
    u32 next_keyframe = m_current_keyframe + 1;
    if (next_keyframe >= m_keyframes.size()) {
        // Finished
        m_playing = false;
        m_complete = true;
        if (m_on_complete) {
            m_on_complete();
        }
        return;
    }

    const auto& from = m_keyframes[m_current_keyframe];
    const auto& to = m_keyframes[next_keyframe];

    // Update time
    m_keyframe_time += dt;

    // Calculate interpolation factor
    float t = 0.0f;
    if (to.duration > 0.0f) {
        t = std::clamp(m_keyframe_time / to.duration, 0.0f, 1.0f);
    }

    // Apply easing
    float eased_t = apply_easing(t, to.move_type);

    // Interpolate
    m_current_position = glm::mix(from.position, to.position, eased_t);
    m_current_target = glm::mix(from.target, to.target, eased_t);
    m_current_fov = glm::mix(from.fov, to.fov, eased_t);

    // Check if we should move to next keyframe
    if (m_keyframe_time >= to.duration) {
        m_current_keyframe++;
        m_keyframe_time = 0.0f;
    }
}

// ============================================================================
// View/Projection
// ============================================================================

glm::mat4 CinematicCamera::view_matrix() const {
    glm::vec3 shaken_pos = m_current_position + m_shake_offset;
    return glm::lookAt(shaken_pos, m_current_target, glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::mat4 CinematicCamera::projection_matrix(float aspect_ratio) const {
    return glm::perspective(glm::radians(m_current_fov), aspect_ratio, 0.1f, 1000.0f);
}

// ============================================================================
// Letterbox
// ============================================================================

float CinematicCamera::letterbox_bar_height(float screen_aspect) const {
    if (!m_letterbox_enabled)
        return 0.0f;

    // Calculate how much we need to crop
    // If screen is 16:9 (1.77) and letterbox is 2.39:1
    // We need bars at top/bottom
    if (screen_aspect < m_letterbox_ratio) {
        // Screen is narrower - bars on top/bottom
        float target_height = 1.0f / m_letterbox_ratio * screen_aspect;
        return (1.0f - target_height) / 2.0f;
    }
    return 0.0f;
}

// ============================================================================
// Camera Shake
// ============================================================================

void CinematicCamera::shake(float intensity, float duration, float frequency) {
    m_shake_intensity = intensity;
    m_shake_duration = duration;
    m_shake_time = 0.0f;
    m_shake_frequency = frequency;
}

void CinematicCamera::update_shake(float dt) {
    if (m_shake_duration <= 0.0f) {
        m_shake_offset = glm::vec3(0.0f);
        return;
    }

    m_shake_time += dt;
    if (m_shake_time >= m_shake_duration) {
        m_shake_duration = 0.0f;
        m_shake_offset = glm::vec3(0.0f);
        return;
    }

    // Decay intensity over time
    float decay = 1.0f - (m_shake_time / m_shake_duration);
    float current_intensity = m_shake_intensity * decay;

    // Perlin-like noise using sin
    float t = m_shake_time * m_shake_frequency;
    m_shake_offset =
        glm::vec3(std::sin(t * 1.0f) * std::cos(t * 0.7f), std::sin(t * 1.3f) * std::cos(t * 0.9f),
                  std::sin(t * 0.8f) * std::cos(t * 1.1f)) *
        current_intensity;
}

// ============================================================================
// Easing Functions
// ============================================================================

float CinematicCamera::apply_easing(float t, CameraMoveType type) const {
    switch (type) {
    case CameraMoveType::Cut:
        return 1.0f; // Instant
    case CameraMoveType::Lerp:
        return t; // Linear
    case CameraMoveType::EaseIn:
        return t * t; // Quadratic ease in
    case CameraMoveType::EaseOut:
        return 1.0f - (1.0f - t) * (1.0f - t); // Quadratic ease out
    case CameraMoveType::EaseInOut: {
        // Smoothstep
        return t * t * (3.0f - 2.0f * t);
    }
    case CameraMoveType::Dolly:
    case CameraMoveType::Orbit:
        return t * t * (3.0f - 2.0f * t); // Same as EaseInOut for now
    }
    return t;
}

} // namespace hz
