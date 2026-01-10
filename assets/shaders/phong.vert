#version 410 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_texcoord;
layout(location = 3) in vec3 a_tangent;

uniform mat4 u_model;
uniform mat4 u_view_projection;
uniform mat4 u_light_space_matrix;

uniform float u_uv_scale;

out vec3 v_frag_pos;
out vec2 v_texcoord;
out vec4 v_frag_pos_light_space;
out mat3 v_TBN; // Tangent-Bitangent-Normal matrix

void main() {
    vec4 world_pos = u_model * vec4(a_position, 1.0);
    v_frag_pos = vec3(world_pos);
    
    // Compute TBN matrix for normal mapping
    mat3 normal_matrix = mat3(transpose(inverse(u_model)));
    vec3 T = normalize(normal_matrix * a_tangent);
    vec3 N = normalize(normal_matrix * a_normal);
    // Re-orthogonalize T with respect to N (Gram-Schmidt)
    T = normalize(T - dot(T, N) * N);
    // Compute bitangent
    vec3 B = cross(N, T);
    
    v_TBN = mat3(T, B, N);
    
    v_texcoord = a_texcoord * u_uv_scale;
    v_frag_pos_light_space = u_light_space_matrix * world_pos;
    gl_Position = u_view_projection * world_pos;
}
