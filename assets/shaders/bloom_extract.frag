#version 410 core

out vec4 frag_color;

in vec2 v_texcoord;

uniform sampler2D u_scene;
uniform float u_threshold;

void main() {
    vec3 color = texture(u_scene, v_texcoord).rgb;
    
    // Calculate brightness using perceived luminance
    float brightness = dot(color, vec3(0.2126, 0.7152, 0.0722));
    
    // Extract bright pixels
    if (brightness > u_threshold) {
        frag_color = vec4(color, 1.0);
    } else {
        frag_color = vec4(0.0, 0.0, 0.0, 1.0);
    }
}
