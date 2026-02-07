#version 450

/**
 * Deferred Geometry Pass - Vertex Shader (Vulkan)
 *
 * Transforms vertices using push constant model matrix + UBO camera data.
 * Outputs position, normal, UV, tangent to GBuffer fragment shader.
 */

// Vertex attributes matching hz::Vertex struct (80 bytes)
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;
layout(location = 3) in vec4 a_Tangent;
layout(location = 4) in ivec4 a_BoneIds;
layout(location = 5) in vec4 a_BoneWeights;

// Camera UBO (set 0, binding 0)
layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 projection;
    mat4 view_projection;
    vec4 camera_position; // xyz = pos, w = unused
} camera;

// Per-object model matrix via push constants (64 bytes)
layout(push_constant) uniform PushConstants {
    mat4 model;
} pc;

// Outputs to fragment shader
layout(location = 0) out vec3 v_WorldPos;
layout(location = 1) out vec3 v_Normal;
layout(location = 2) out vec2 v_TexCoord;
layout(location = 3) out vec4 v_Tangent;

void main() {
    vec4 worldPos = pc.model * vec4(a_Position, 1.0);
    v_WorldPos = worldPos.xyz;

    mat3 normalMatrix = transpose(inverse(mat3(pc.model)));
    v_Normal = normalize(normalMatrix * a_Normal);
    v_TexCoord = a_TexCoord;
    v_Tangent = vec4(normalize(normalMatrix * a_Tangent.xyz), a_Tangent.w);

    gl_Position = camera.view_projection * worldPos;
}
