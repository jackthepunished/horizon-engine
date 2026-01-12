#version 410 core

layout(location = 0) in vec3 a_position;  // Quad vertex position
layout(location = 1) in vec2 a_texcoord;

// Instance data
layout(location = 3) in vec3 a_instance_position;  // World position
layout(location = 4) in vec2 a_instance_size;      // Width, Height
layout(location = 5) in vec4 a_instance_color;     // Tint color

#include "common/camera.glsl"

out vec2 v_texcoord;
out vec4 v_color;
out vec3 v_world_pos;

void main() {
    v_texcoord = a_texcoord;
    v_color = a_instance_color;
    
    // Extract camera right and up vectors from view matrix
    // View matrix columns are: right, up, forward (transposed)
    vec3 camera_right = vec3(u_view[0][0], u_view[1][0], u_view[2][0]);
    vec3 camera_up = vec3(0.0, 1.0, 0.0); // Keep trees upright (cylindrical billboard)
    
    // Scale the quad
    vec3 scaled_pos = a_position;
    scaled_pos.x *= a_instance_size.x;
    scaled_pos.y *= a_instance_size.y;
    
    // Billboard: offset from instance position using camera vectors
    vec3 world_pos = a_instance_position 
                   + camera_right * scaled_pos.x 
                   + camera_up * scaled_pos.y;
    
    v_world_pos = world_pos;
    
    gl_Position = u_view_projection * vec4(world_pos, 1.0);
}
