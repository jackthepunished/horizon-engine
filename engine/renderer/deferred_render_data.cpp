#include "deferred_render_data.hpp"

namespace hz {

DeferredCameraUBO make_deferred_camera_ubo(const glm::mat4& view, const glm::mat4& projection,
                                           const glm::vec3& camera_position) {
    DeferredCameraUBO ubo{};
    ubo.view = view;
    ubo.projection = projection;
    ubo.view_projection = projection * view;
    ubo.camera_position = glm::vec4(camera_position, 1.0f);
    return ubo;
}

DeferredLightUBO make_deferred_light_ubo(const glm::vec3& sun_direction, const glm::vec3& sun_color,
                                         f32 sun_intensity, u32 point_light_count,
                                         u32 spot_light_count) {
    DeferredLightUBO ubo{};
    ubo.sun_direction = glm::vec4(sun_direction, 0.0f);
    ubo.sun_color = glm::vec4(sun_color, sun_intensity);
    ubo.light_counts = glm::uvec4(point_light_count, spot_light_count, 0u, 0u);
    return ubo;
}

} // namespace hz
