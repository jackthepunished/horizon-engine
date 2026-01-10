#version 410 core

out vec4 frag_color;

in vec2 v_texcoord;

uniform sampler2D u_image;
uniform bool u_horizontal;

// Gaussian weights for 9-tap blur
const float weight[5] = float[] (0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);

void main() {
    vec2 tex_offset = 1.0 / textureSize(u_image, 0); // Size of single texel
    vec3 result = texture(u_image, v_texcoord).rgb * weight[0]; // Current fragment
    
    if (u_horizontal) {
        for (int i = 1; i < 5; ++i) {
            result += texture(u_image, v_texcoord + vec2(tex_offset.x * i, 0.0)).rgb * weight[i];
            result += texture(u_image, v_texcoord - vec2(tex_offset.x * i, 0.0)).rgb * weight[i];
        }
    } else {
        for (int i = 1; i < 5; ++i) {
            result += texture(u_image, v_texcoord + vec2(0.0, tex_offset.y * i)).rgb * weight[i];
            result += texture(u_image, v_texcoord - vec2(0.0, tex_offset.y * i)).rgb * weight[i];
        }
    }
    
    frag_color = vec4(result, 1.0);
}
