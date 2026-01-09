#include "camera.hpp"

#include <cmath>

namespace hz {

Camera::Camera(glm::vec3 position, glm::vec3 up, f32 yaw, f32 pitch)
    : m_position(position), m_world_up(up), m_yaw(yaw), m_pitch(pitch) {
    update_vectors();
}

glm::mat4 Camera::view_matrix() const {
    return glm::lookAt(m_position, m_position + m_front, m_up);
}

glm::mat4 Camera::projection_matrix(f32 aspect_ratio) const {
    return glm::perspective(glm::radians(fov), aspect_ratio, near_plane, far_plane);
}

void Camera::process_movement(glm::vec3 direction, f32 delta_time) {
    f32 velocity = movement_speed * delta_time;

    // Move in the XZ plane (ignore Y for ground movement)
    glm::vec3 front_xz = glm::normalize(glm::vec3(m_front.x, 0.0f, m_front.z));
    glm::vec3 right_xz = glm::normalize(glm::vec3(m_right.x, 0.0f, m_right.z));

    m_position += front_xz * direction.z * velocity;
    m_position += right_xz * direction.x * velocity;
    m_position.y += direction.y * velocity;

    // Clamp to minimum height (eye level above ground)
    constexpr f32 MIN_HEIGHT = 1.7f;
    if (m_position.y < MIN_HEIGHT) {
        m_position.y = MIN_HEIGHT;
    }
}

void Camera::process_mouse(f32 x_offset, f32 y_offset, bool constrain_pitch) {
    x_offset *= mouse_sensitivity;
    y_offset *= mouse_sensitivity;

    m_yaw += x_offset;
    m_pitch += y_offset;

    // Constrain pitch to avoid flipping
    if (constrain_pitch) {
        if (m_pitch > 89.0f)
            m_pitch = 89.0f;
        if (m_pitch < -89.0f)
            m_pitch = -89.0f;
    }

    update_vectors();
}

void Camera::update_vectors() {
    // Calculate new front vector
    glm::vec3 new_front;
    new_front.x = std::cos(glm::radians(m_yaw)) * std::cos(glm::radians(m_pitch));
    new_front.y = std::sin(glm::radians(m_pitch));
    new_front.z = std::sin(glm::radians(m_yaw)) * std::cos(glm::radians(m_pitch));
    m_front = glm::normalize(new_front);

    // Recalculate right and up vectors
    m_right = glm::normalize(glm::cross(m_front, m_world_up));
    m_up = glm::normalize(glm::cross(m_right, m_front));
}

} // namespace hz
