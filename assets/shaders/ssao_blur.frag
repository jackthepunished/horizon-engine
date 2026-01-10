#version 410 core

out float g_occlusion;

in vec2 v_texcoord;

uniform sampler2D u_ssao_input;

void main() {
    float result = 0.0;
    vec2 texel_size = 1.0 / vec2(textureSize(u_ssao_input, 0));
    
    // 4x4 Box Blur
    for (int x = -2; x < 2; ++x) {
        for (int y = -2; y < 2; ++y) {
            vec2 offset = vec2(float(x), float(y)) * texel_size;
            result += texture(u_ssao_input, v_texcoord + offset).r;
        }
    }
    
    g_occlusion = result / (4.0 * 4.0);
}
