#version 410 core

layout(location = 0) in vec3 a_position;

out vec3 v_texcoord;

uniform mat4 u_projection;
uniform mat4 u_view;

void main() {
    v_texcoord = a_position;
    // Remove translation from view matrix (skybox stays at origin)
    mat4 view_no_translation = mat4(mat3(u_view));
    vec4 pos = u_projection * view_no_translation * vec4(a_position, 1.0);
    // Set z = w so depth is always 1.0 (furthest)
    gl_Position = pos.xyww;
}
