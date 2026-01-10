#version 410 core
out vec4 frag_color;

in vec2 v_texcoord;

uniform sampler2D u_hdr_buffer;
uniform sampler2D u_bloom_blur;
uniform float u_exposure;
uniform float u_bloom_intensity;
uniform bool u_bloom_enabled;

void main() {
    const float gamma = 2.2;
    vec3 hdr_color = texture(u_hdr_buffer, v_texcoord).rgb;

    // Add bloom
    if (u_bloom_enabled) {
        vec3 bloom_color = texture(u_bloom_blur, v_texcoord).rgb;
        hdr_color += bloom_color * u_bloom_intensity;
    }

    // Exposure tone mapping (preferred for adjustment)
    vec3 mapped = vec3(1.0) - exp(-hdr_color * u_exposure);

    // Gamma correction
    mapped = pow(mapped, vec3(1.0 / gamma));

    frag_color = vec4(mapped, 1.0);
}
