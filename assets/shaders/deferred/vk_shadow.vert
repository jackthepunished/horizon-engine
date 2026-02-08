#version 450

/**
 * Shadow Pass - Vertex Shader (Vulkan)
 *
 * Renders geometry to shadow map (depth only).
 */

layout(location = 0) in vec3 a_Position;

// MVP matrix via push constants
layout(push_constant) uniform PushConstants {
    mat4 mvp;
} pc;

void main() {
    gl_Position = pc.mvp * vec4(a_Position, 1.0);
}
