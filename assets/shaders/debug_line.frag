#version 410 core

// Debug line fragment shader
// Simple unlit color output

in vec3 v_Color;

out vec4 FragColor;

void main() {
    FragColor = vec4(v_Color, 1.0);
}
