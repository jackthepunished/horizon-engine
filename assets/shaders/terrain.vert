#version 410 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_texcoord;    // Tiled UV for detail textures
layout(location = 3) in vec2 a_splatcoord;  // 0-1 UV for splatmap

#include "common/camera.glsl"

uniform mat4 u_model;
uniform mat4 u_light_space_matrix;

out vec3 v_world_pos;
out vec3 v_normal;
out vec2 v_texcoord;
out vec2 v_splatcoord;
out vec4 v_frag_pos_light_space;

void main() {
    vec4 world_pos = u_model * vec4(a_position, 1.0);
    v_world_pos = world_pos.xyz;
    
    // Transform normal to world space
    mat3 normal_matrix = mat3(transpose(inverse(u_model)));
    v_normal = normalize(normal_matrix * a_normal);
    
    v_texcoord = a_texcoord;
    v_splatcoord = a_splatcoord;
    v_frag_pos_light_space = u_light_space_matrix * world_pos;
    
    gl_Position = u_view_projection * world_pos;
}
