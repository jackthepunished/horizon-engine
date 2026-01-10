#version 410 core

out vec4 frag_color;

in vec3 v_local_pos;

uniform sampler2D u_equirect_map;

const vec2 INV_ATAN = vec2(0.1591, 0.3183); // 1/(2*PI), 1/PI

vec2 sample_spherical_map(vec3 v) {
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    uv *= INV_ATAN;
    uv += 0.5;
    return uv;
}

void main() {
    vec2 uv = sample_spherical_map(normalize(v_local_pos));
    vec3 color = texture(u_equirect_map, uv).rgb;
    
    frag_color = vec4(color, 1.0);
}
