#version 450

/**
 * Fullscreen Quad - Vertex Shader (Vulkan)
 *
 * Simple pass-through for fullscreen effects (lighting, composite, post-process).
 * Vertex buffer layout: vec3 position + vec2 texcoord (stride 20 bytes).
 */

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec2 a_TexCoord;

layout(location = 0) out vec2 v_TexCoord;

void main() {
    v_TexCoord  = a_TexCoord;
    gl_Position = vec4(a_Position, 1.0);
}
