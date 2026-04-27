#version 450

/**
 * Deferred Geometry Pass - GPU-Driven Vertex Shader
 *
 * Same outputs as vk_geometry.vert, but reads the per-object model matrix
 * from a storage buffer indexed by gl_BaseInstance (set by the cull shader's
 * generated indirect draw command).
 */

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
    vec4 camera_position;
} camera;

// Per-object data SSBO (set 2, binding 0). Layout must match GPUObjectData.
struct GPUObjectData {
    mat4 model;
    mat4 inverse_transpose_model;
    vec4 bounding_sphere;
    uint mesh_index;
    uint material_index;
    uint flags;
    uint _padding;
};

layout(std430, set = 2, binding = 0) readonly buffer ObjectBuffer {
    GPUObjectData objects[];
};

// Outputs to fragment shader
layout(location = 0) out vec3 v_WorldPos;
layout(location = 1) out vec3 v_Normal;
layout(location = 2) out vec2 v_TexCoord;
layout(location = 3) out vec4 v_Tangent;
layout(location = 4) flat out uint v_ObjectId;

void main() {
    GPUObjectData obj = objects[gl_BaseInstance];

    vec4 worldPos = obj.model * vec4(a_Position, 1.0);
    v_WorldPos = worldPos.xyz;

    mat3 normalMatrix = mat3(obj.inverse_transpose_model);
    v_Normal = normalize(normalMatrix * a_Normal);
    v_TexCoord = a_TexCoord;
    v_Tangent = vec4(normalize(normalMatrix * a_Tangent.xyz), a_Tangent.w);
    v_ObjectId = gl_BaseInstance;

    gl_Position = camera.view_projection * worldPos;
}
