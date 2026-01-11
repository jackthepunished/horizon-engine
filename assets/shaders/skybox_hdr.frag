#version 410 core

out vec4 frag_color;

in vec3 v_world_pos;

uniform samplerCube u_skybox_map;
uniform bool u_use_texture;
uniform vec3 u_top_color;
uniform vec3 u_bottom_color;

void main() {
    if (u_use_texture) {
        vec3 dir = normalize(v_world_pos);
        // Correct for OpenGL cubemap coordinate system if needed, 
        // but IBL generation should have handled it.
        vec3 color = texture(u_skybox_map, dir).rgb;
        frag_color = vec4(color, 1.0);
    } else {
        // Procedural sky gradient
        vec3 dir = normalize(v_world_pos);
        float factor = clamp(dir.y * 0.5 + 0.5, 0.0, 1.0);
        vec3 color = mix(u_bottom_color, u_top_color, factor);
        frag_color = vec4(color, 1.0);
    }
}
