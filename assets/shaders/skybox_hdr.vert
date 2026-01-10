#version 410 core

layout (location = 0) in vec3 a_position;
layout (location = 1) in vec3 a_normal;
layout (location = 2) in vec2 a_texcoord;

uniform mat4 u_view;
uniform mat4 u_projection;

out vec3 v_world_pos;

void main() {
    v_world_pos = a_position;
    // Remove translation from view matrix for skybox
    mat4 rot_view = mat4(mat3(u_view));
    vec4 clip_pos = u_projection * rot_view * vec4(a_position, 1.0);
    gl_Position = clip_pos.xyww; // Force z to w (max depth = 1.0)
}
