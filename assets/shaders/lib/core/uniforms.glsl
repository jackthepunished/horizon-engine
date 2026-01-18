/**
 * =============================================================================
 * HORIZON ENGINE - SHADER LIBRARY v2.0
 * =============================================================================
 * 
 * File: core/uniforms.glsl
 * Purpose: All uniform buffer object definitions
 * =============================================================================
 */

#ifndef HZ_UNIFORMS_GLSL
#define HZ_UNIFORMS_GLSL

#include "defines.glsl"

// =============================================================================
// CAMERA DATA (Binding Point 0)
// =============================================================================

layout(std140) uniform CameraData {
    mat4 hz_View;
    mat4 hz_Projection;
    mat4 hz_ViewProjection;
    mat4 hz_InverseView;
    mat4 hz_InverseProjection;
    mat4 hz_InverseViewProjection;
    mat4 hz_PrevViewProjection;      // For TAA/motion vectors
    vec4 hz_CameraPosition;          // xyz = position, w = unused
    vec4 hz_CameraDirection;         // xyz = forward, w = unused
    vec4 hz_ViewportSize;            // xy = size, zw = 1/size
    vec4 hz_ClipPlanes;              // x = near, y = far, z = 1/near, w = 1/far
    vec4 hz_JitterOffset;            // xy = current jitter, zw = prev jitter
} hz_Camera;

// =============================================================================
// LIGHT STRUCTURES
// =============================================================================

struct DirectionalLight {
    vec4 direction;     // xyz = direction, w = shadow enabled
    vec4 color;         // xyz = color, w = shadow bias
    vec4 intensity;     // x = intensity, y = shadow strength, zw = unused
};

struct PointLight {
    vec4 positionRadius;    // xyz = position, w = radius
    vec4 colorIntensity;    // xyz = color, w = intensity
};

struct SpotLight {
    vec4 positionRadius;    // xyz = position, w = radius
    vec4 directionCutoff;   // xyz = direction, w = cos(inner cutoff)
    vec4 colorIntensity;    // xyz = color, w = intensity
    vec4 params;            // x = cos(outer cutoff), y = shadow index, zw = unused
};

// =============================================================================
// SCENE DATA (Binding Point 1)
// =============================================================================

layout(std140) uniform SceneData {
    DirectionalLight hz_Sun;
    vec4 hz_AmbientColor;            // xyz = ambient, w = ambient intensity
    vec4 hz_TimeParams;              // x = time, y = sin(time), z = cos(time), w = deltaTime
    vec4 hz_FogParams;               // x = density, y = gradient, z = start, w = enabled
    vec4 hz_FogColor;
    int hz_PointLightCount;
    int hz_SpotLightCount;
    int hz_FrameIndex;
    float hz_Exposure;
    PointLight hz_PointLights[MAX_POINT_LIGHTS];
} hz_Scene;

// =============================================================================
// SHADOW DATA (Binding Point 2)
// =============================================================================

layout(std140) uniform ShadowData {
    mat4 hz_LightSpaceMatrices[MAX_CASCADES];
    vec4 hz_CascadeSplits;           // Split depths for 4 cascades
    vec4 hz_ShadowParams;            // x = bias, y = normal bias, z = softness, w = cascade count
    vec4 hz_ShadowMapSize;           // xy = size, zw = 1/size
} hz_Shadow;

// =============================================================================
// MATERIAL DATA (Push constant or per-draw UBO)
// =============================================================================

struct MaterialData {
    vec4 baseColorFactor;
    vec4 emissiveFactor;             // xyz = color, w = strength
    float metallicFactor;
    float roughnessFactor;
    float normalScale;
    float occlusionStrength;
    float alphaCutoff;
    float parallaxScale;
    int flags;                       // Bit flags for features
    int _pad;
};

// Material flags
#define MAT_FLAG_ALBEDO_MAP       (1 << 0)
#define MAT_FLAG_NORMAL_MAP       (1 << 1)
#define MAT_FLAG_METALLIC_MAP     (1 << 2)
#define MAT_FLAG_ROUGHNESS_MAP    (1 << 3)
#define MAT_FLAG_AO_MAP           (1 << 4)
#define MAT_FLAG_EMISSIVE_MAP     (1 << 5)
#define MAT_FLAG_HEIGHT_MAP       (1 << 6)
#define MAT_FLAG_ALPHA_CUTOUT     (1 << 7)
#define MAT_FLAG_DOUBLE_SIDED     (1 << 8)
#define MAT_FLAG_UNLIT            (1 << 9)

#define HAS_FLAG(mat, flag)       (((mat).flags & (flag)) != 0)

// =============================================================================
// SKINNING DATA (Binding Point 3)
// =============================================================================

#ifdef HZ_FEATURE_SKINNING
layout(std140) uniform SkinningData {
    mat4 hz_BoneMatrices[MAX_BONES];
    int hz_BoneCount;
    int _pad[3];
} hz_Skinning;
#endif

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

// Reconstruct world position from depth
vec3 hz_ReconstructWorldPos(vec2 uv, float depth) {
    vec4 clip = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 view = hz_Camera.hz_InverseProjection * clip;
    view /= view.w;
    vec4 world = hz_Camera.hz_InverseView * view;
    return world.xyz;
}

// Get linear depth from depth buffer
float hz_LinearDepth(float depth) {
    float near = hz_Camera.hz_ClipPlanes.x;
    float far = hz_Camera.hz_ClipPlanes.y;
    return (2.0 * near * far) / (far + near - depth * (far - near));
}

// Get time values
float hz_Time()      { return hz_Scene.hz_TimeParams.x; }
float hz_SinTime()   { return hz_Scene.hz_TimeParams.y; }
float hz_CosTime()   { return hz_Scene.hz_TimeParams.z; }
float hz_DeltaTime() { return hz_Scene.hz_TimeParams.w; }

#endif // HZ_UNIFORMS_GLSL
