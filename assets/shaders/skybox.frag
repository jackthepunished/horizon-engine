#version 410 core

in vec3 v_texcoord;

out vec4 frag_color;

uniform samplerCube u_skybox;

void main() {
    frag_color = texture(u_skybox, v_texcoord);
}
