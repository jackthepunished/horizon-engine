// Basic grid shader - fragment
// Language: GLSL 410 Core
#version 410 core

in vec3 v_world_pos;
in vec3 v_normal;
in vec2 v_texcoord;

out vec4 frag_color;

uniform vec3 u_camera_pos;

// Grid pattern function
float grid(vec2 pos, float scale) {
    vec2 grid_pos = fract(pos * scale);
    float line_width = 0.02 * scale;
    float x_line = step(grid_pos.x, line_width) + step(1.0 - line_width, grid_pos.x);
    float z_line = step(grid_pos.y, line_width) + step(1.0 - line_width, grid_pos.y);
    return max(x_line, z_line);
}

void main() {
    // Base color: dark gray
    vec3 base_color = vec3(0.15, 0.15, 0.18);

    // Grid lines: lighter
    float grid_small = grid(v_world_pos.xz, 1.0);
    float grid_large = grid(v_world_pos.xz, 0.1);

    vec3 color = base_color;
    color = mix(color, vec3(0.25, 0.25, 0.3), grid_small * 0.5);
    color = mix(color, vec3(0.4, 0.4, 0.5), grid_large * 0.8);

    // Fade out at distance
    float dist = length(v_world_pos.xz - u_camera_pos.xz);
    float fade = 1.0 - smoothstep(10.0, 50.0, dist);

    // Simple fog
    float fog_dist = length(v_world_pos - u_camera_pos);
    float fog = 1.0 - exp(-0.01 * fog_dist);
    vec3 fog_color = vec3(0.2, 0.3, 0.5);
    color = mix(color, fog_color, fog * 0.5);

    frag_color = vec4(color, fade);
}
