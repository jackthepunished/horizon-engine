#version 410 core

out vec4 frag_color;

uniform vec3 u_outline_color;

void main() {
    frag_color = vec4(u_outline_color, 1.0);
}
