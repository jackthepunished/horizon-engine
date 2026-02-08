#pragma once

/**
 * @file deferred_render_data.hpp
 * @brief GPU data layouts and helpers for the deferred renderer
 */

#include "engine/core/types.hpp"

#include <glm/glm.hpp>

namespace hz {

// Matches std140 layout used by vk_geometry.vert / vk_lighting.frag
struct alignas(16) DeferredCameraUBO {
    glm::mat4 view;
    glm::mat4 projection;
    glm::mat4 view_projection;
    glm::vec4 camera_position; // xyz = position, w = unused
};

// Matches std140 layout used by vk_lighting.frag
struct alignas(16) DeferredLightUBO {
    glm::vec4 sun_direction; // xyz = direction, w = unused
    glm::vec4 sun_color;     // xyz = color, w = intensity
    glm::uvec4 light_counts; // x = point count, y = spot count, z/w = reserved
};

// Matches std140 layout used by vk_lighting.frag
struct alignas(16) DeferredShadowUBO {
    glm::mat4 light_space_matrices[4];
    glm::vec4 cascade_splits; // x,y,z,w = split depths (in view space z)
    glm::vec4 params;         // x=enabled, y=soft_shadows, z=shadow_bias, w=unused
};

constexpr u32 kMaxDeferredPointLights = 256;
constexpr u32 kMaxDeferredSpotLights = 256;

[[nodiscard]] DeferredCameraUBO make_deferred_camera_ubo(const glm::mat4& view,
                                                         const glm::mat4& projection,
                                                         const glm::vec3& camera_position);

[[nodiscard]] DeferredLightUBO make_deferred_light_ubo(const glm::vec3& sun_direction,
                                                       const glm::vec3& sun_color,
                                                       f32 sun_intensity, u32 point_light_count,
                                                       u32 spot_light_count);

} // namespace hz
