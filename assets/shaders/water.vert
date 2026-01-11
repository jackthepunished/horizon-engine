#version 410 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec2 a_texcoord;

#include "common/camera.glsl"
#include "common/scene_data.glsl"

uniform mat4 u_model;
// u_time is from SceneData UBO
uniform float u_wave_strength;
uniform float u_wave_speed;

out vec3 v_world_pos;
out vec2 v_texcoord;
out vec4 v_clip_space;
out vec3 v_to_camera;

// Simple wave function using multiple sine waves
float wave(vec2 pos, float time) {
    float w1 = sin(pos.x * 0.5 + time * u_wave_speed) * 0.3;
    float w2 = sin(pos.y * 0.7 + time * u_wave_speed * 0.8) * 0.2;
    float w3 = sin((pos.x + pos.y) * 0.3 + time * u_wave_speed * 1.2) * 0.25;
    float w4 = sin((pos.x - pos.y) * 0.4 + time * u_wave_speed * 0.6) * 0.15;
    return (w1 + w2 + w3 + w4) * u_wave_strength;
}

void main() {
    vec3 pos = a_position;
    
    // Apply wave displacement
    pos.y += wave(a_position.xz, u_time);
    
    vec4 world_pos = u_model * vec4(pos, 1.0);
    v_world_pos = world_pos.xyz;
    v_texcoord = a_texcoord;
    v_to_camera = u_view_pos - world_pos.xyz;
    
    v_clip_space = u_view_projection * world_pos;
    gl_Position = v_clip_space;
}
