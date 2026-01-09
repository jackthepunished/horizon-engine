#pragma once

/**
 * @file camera.hpp
 * @brief FPS Camera system for 3D rendering
 */

#include "engine/core/types.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace hz {

/**
 * @brief FPS-style camera with mouse look and WASD movement
 */
class Camera {
public:
    Camera(glm::vec3 position = glm::vec3(0.0f, 2.0f, 5.0f),
           glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f), f32 yaw = -90.0f, f32 pitch = 0.0f);

    /**
     * @brief Get the view matrix
     */
    [[nodiscard]] glm::mat4 view_matrix() const;

    /**
     * @brief Get the projection matrix
     */
    [[nodiscard]] glm::mat4 projection_matrix(f32 aspect_ratio) const;

    /**
     * @brief Process keyboard movement
     */
    void process_movement(glm::vec3 direction, f32 delta_time);

    /**
     * @brief Process mouse look
     */
    void process_mouse(f32 x_offset, f32 y_offset, bool constrain_pitch = true);

    /**
     * @brief Get camera position
     */
    [[nodiscard]] glm::vec3 position() const noexcept { return m_position; }

    /**
     * @brief Set camera position
     */
    void set_position(const glm::vec3& pos) { m_position = pos; }

    /**
     * @brief Get camera front vector
     */
    [[nodiscard]] glm::vec3 front() const noexcept { return m_front; }

    /**
     * @brief Get camera right vector
     */
    [[nodiscard]] glm::vec3 right() const noexcept { return m_right; }

    // Settings
    f32 movement_speed{5.0f};
    f32 mouse_sensitivity{0.1f};
    f32 fov{45.0f};
    f32 near_plane{0.1f};
    f32 far_plane{1000.0f};

private:
    void update_vectors();

    glm::vec3 m_position;
    glm::vec3 m_front{0.0f, 0.0f, -1.0f};
    glm::vec3 m_up{0.0f, 1.0f, 0.0f};
    glm::vec3 m_right{1.0f, 0.0f, 0.0f};
    glm::vec3 m_world_up;

    f32 m_yaw;
    f32 m_pitch;
};

} // namespace hz
