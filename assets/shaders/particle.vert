#version 410 core

// Quad vertex
layout(location = 0) in vec3 a_position;
layout(location = 1) in vec2 a_texcoord;

// Per-instance data
layout(location = 2) in vec3 a_instance_position;
layout(location = 3) in vec4 a_instance_color;
layout(location = 4) in float a_instance_size;
layout(location = 5) in float a_instance_rotation;

#include "common/camera.glsl"

out vec2 v_texcoord;
out vec4 v_color;

mat2 rotation2d(float angle) {
    float s = sin(angle);
    float c = cos(angle);
    return mat2(c, -s, s, c);
}

void main() {
    v_texcoord = a_texcoord;
    v_color = a_instance_color;
    
    // Billboard: extract right and up vectors from view matrix
    vec3 camera_right = vec3(u_view[0][0], u_view[1][0], u_view[2][0]);
    vec3 camera_up = vec3(u_view[0][1], u_view[1][1], u_view[2][1]);
    
    // Apply rotation to local position
    vec2 rotated_pos = rotation2d(a_instance_rotation) * a_position.xy;
    
    // Scale and orient towards camera
    vec3 world_pos = a_instance_position 
                   + camera_right * rotated_pos.x * a_instance_size
                   + camera_up * rotated_pos.y * a_instance_size;
    
    gl_Position = u_view_projection * vec4(world_pos, 1.0);
}
