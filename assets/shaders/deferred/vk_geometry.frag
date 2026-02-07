#version 450

/**
 * Deferred Geometry Pass - Fragment Shader (Vulkan)
 *
 * Writes surface attributes to multiple GBuffer render targets:
 *   RT0 (RGBA16F): RGB = Albedo,   A = Metallic
 *   RT1 (RGBA16F): RG  = Normal (octahedron), B = Roughness, A = AO
 *   RT2 (RGBA16F): RGB = Emission, A = Material ID (normalized)
 *   RT3 (RG16F):   RG  = Velocity (zero for static objects)
 */

// Inputs from vertex shader
layout(location = 0) in vec3 v_WorldPos;
layout(location = 1) in vec3 v_Normal;
layout(location = 2) in vec2 v_TexCoord;
layout(location = 3) in vec4 v_Tangent;

// Material textures (set 1)
layout(set = 1, binding = 0) uniform sampler2D u_AlbedoMap;
layout(set = 1, binding = 1) uniform sampler2D u_NormalMap;
layout(set = 1, binding = 2) uniform sampler2D u_ARMMap; // AO, Roughness, Metallic packed

// GBuffer outputs
layout(location = 0) out vec4 gAlbedoMetallic;
layout(location = 1) out vec4 gNormalRoughness;
layout(location = 2) out vec4 gEmissionID;
layout(location = 3) out vec2 gVelocity;

// --------------------------------------------------------------------------
// Octahedron normal encoding [-1,1] -> [0,1]
// --------------------------------------------------------------------------
vec2 encode_octahedron(vec3 n) {
    n /= (abs(n.x) + abs(n.y) + abs(n.z));
    if (n.z < 0.0) {
        n.xy = (1.0 - abs(n.yx)) * vec2(
            n.x >= 0.0 ? 1.0 : -1.0,
            n.y >= 0.0 ? 1.0 : -1.0
        );
    }
    return n.xy * 0.5 + 0.5;
}

// --------------------------------------------------------------------------
// Normal mapping via TBN matrix
// --------------------------------------------------------------------------
vec3 apply_normal_map(vec3 normal_sample, vec3 N, vec4 T) {
    // Tangent-space normal from map [0,1] -> [-1,1]
    vec3 ts_normal = normal_sample * 2.0 - 1.0;

    vec3 T3 = normalize(T.xyz);
    vec3 N3 = normalize(N);
    // Re-orthogonalize (Gram-Schmidt)
    T3 = normalize(T3 - dot(T3, N3) * N3);
    vec3 B = cross(N3, T3) * T.w; // T.w = handedness

    mat3 TBN = mat3(T3, B, N3);
    return normalize(TBN * ts_normal);
}

void main() {
    // Sample material textures
    vec4 albedo_sample = texture(u_AlbedoMap, v_TexCoord);
    vec3 normal_sample = texture(u_NormalMap, v_TexCoord).rgb;
    vec3 arm_sample    = texture(u_ARMMap, v_TexCoord).rgb;

    // Unpack ARM: R=AO, G=Roughness, B=Metallic
    float ao        = arm_sample.r;
    float roughness = arm_sample.g;
    float metallic  = arm_sample.b;

    // Apply normal mapping
    vec3 world_normal = apply_normal_map(normal_sample, v_Normal, v_Tangent);

    // Encode normal to octahedron
    vec2 encoded_normal = encode_octahedron(world_normal);

    // Write GBuffer
    gAlbedoMetallic  = vec4(albedo_sample.rgb, metallic);
    gNormalRoughness = vec4(encoded_normal, roughness, ao);
    gEmissionID      = vec4(0.0, 0.0, 0.0, 0.0); // No emission for now; material ID = 0
    gVelocity        = vec2(0.0, 0.0);             // Static objects: zero velocity
}
