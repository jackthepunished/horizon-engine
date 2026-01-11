#version 410 core

// Blade quad vertex attributes
layout(location = 0) in vec3 a_position;
layout(location = 1) in vec2 a_texcoord;

// Instance attributes
layout(location = 2) in vec3 a_instance_position;
layout(location = 3) in float a_instance_height;
layout(location = 4) in float a_instance_rotation;
layout(location = 5) in float a_instance_color_var;

#include "common/camera.glsl"

// uniform mat4 u_view_projection; // In CameraData
// uniform vec3 u_camera_pos;      // In CameraData as u_view_pos
uniform float u_time;
uniform float u_wind_strength;
uniform float u_wind_speed;
uniform float u_blade_width;

out vec2 v_texcoord;
out float v_color_variation;
out float v_world_height;
out vec3 v_world_pos;

mat3 rotation_y(float angle) {
    float s = sin(angle);
    float c = cos(angle);
    return mat3(
        c, 0.0, s,
        0.0, 1.0, 0.0,
        -s, 0.0, c
    );
}

void main() {
    v_texcoord = a_texcoord;
    v_color_variation = a_instance_color_var;
    
    // Scale blade
    vec3 local_pos = a_position;
    local_pos.x *= u_blade_width;
    local_pos.y *= a_instance_height;
    
    // Wind animation - affects top of blade more than bottom
    float wind_factor = local_pos.y / a_instance_height; // 0 at bottom, 1 at top
    wind_factor = wind_factor * wind_factor; // Quadratic falloff
    
    // Wind displacement based on world position + time
    float wind_offset = sin(u_time * u_wind_speed + a_instance_position.x * 0.5 + a_instance_position.z * 0.3);
    wind_offset += sin(u_time * u_wind_speed * 0.7 + a_instance_position.z * 0.4) * 0.5;
    
    local_pos.x += wind_offset * wind_factor * u_wind_strength;
    local_pos.z += wind_offset * wind_factor * u_wind_strength * 0.5;
    
    // Apply Y rotation
    local_pos = rotation_y(a_instance_rotation) * local_pos;
    
    // Billboard: make grass face camera (Y-axis rotation only)
    vec3 to_camera = normalize(u_view_pos - a_instance_position);
    float billboard_angle = atan(to_camera.x, to_camera.z);
    local_pos = rotation_y(billboard_angle) * local_pos;
    
    // Final world position
    vec3 world_pos = a_instance_position + local_pos;
    v_world_height = a_instance_position.y;
    v_world_pos = world_pos;
    
    gl_Position = u_view_projection * vec4(world_pos, 1.0);
}
