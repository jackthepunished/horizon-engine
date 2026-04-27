#version 450

/**
 * Deferred Geometry Pass - GPU-Driven Fragment Shader
 *
 * v1: shares the default material set (set 1) across every indirect draw.
 *     Per-object material overrides will return when bindless materials land.
 */

layout(location = 0) in vec3 v_WorldPos;
layout(location = 1) in vec3 v_Normal;
layout(location = 2) in vec2 v_TexCoord;
layout(location = 3) in vec4 v_Tangent;
layout(location = 4) flat in uint v_ObjectId;

layout(set = 1, binding = 0) uniform sampler2D u_AlbedoMap;
layout(set = 1, binding = 1) uniform sampler2D u_NormalMap;
layout(set = 1, binding = 2) uniform sampler2D u_ARMMap;

layout(location = 0) out vec4 gAlbedoMetallic;
layout(location = 1) out vec4 gNormalRoughness;
layout(location = 2) out vec4 gEmissionID;
layout(location = 3) out vec2 gVelocity;

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

vec3 apply_normal_map(vec3 normal_sample, vec3 N, vec4 T) {
    vec3 ts_normal = normal_sample * 2.0 - 1.0;
    vec3 T3 = normalize(T.xyz);
    vec3 N3 = normalize(N);
    T3 = normalize(T3 - dot(T3, N3) * N3);
    vec3 B = cross(N3, T3) * T.w;
    mat3 TBN = mat3(T3, B, N3);
    return normalize(TBN * ts_normal);
}

void main() {
    vec3 albedo_sample = texture(u_AlbedoMap, v_TexCoord).rgb;
    vec3 normal_sample = texture(u_NormalMap, v_TexCoord).rgb;
    vec3 arm_sample    = texture(u_ARMMap, v_TexCoord).rgb;

    float ao        = arm_sample.r;
    float roughness = arm_sample.g;
    float metallic  = arm_sample.b;

    vec3 world_normal = apply_normal_map(normal_sample, v_Normal, v_Tangent);
    vec2 encoded_normal = encode_octahedron(world_normal);

    gAlbedoMetallic  = vec4(albedo_sample, metallic);
    gNormalRoughness = vec4(encoded_normal, roughness, ao);
    gEmissionID      = vec4(0.0, 0.0, 0.0, 0.0);
    gVelocity        = vec2(0.0, 0.0);
}
