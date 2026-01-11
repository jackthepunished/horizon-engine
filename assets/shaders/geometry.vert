#version 410 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_texcoord;

#include "common/camera.glsl"

uniform mat4 u_model;

out vec3 v_view_pos;
out vec3 v_view_normal;

void main() {
    vec4 view_pos = u_view * u_model * vec4(a_position, 1.0);
    v_view_pos = view_pos.xyz;
    
    mat3 normal_matrix = transpose(inverse(mat3(u_view * u_model)));
    v_view_normal = normal_matrix * a_normal;
    
    gl_Position = u_projection * view_pos;
}
