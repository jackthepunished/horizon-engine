/**
 * =============================================================================
 * HORIZON ENGINE - SHADER LIBRARY v2.0
 * =============================================================================
 * 
 * File: lighting/shadows.glsl
 * Purpose: Shadow mapping functions (CSM, PCF, PCSS)
 * =============================================================================
 */

#ifndef HZ_SHADOWS_GLSL
#define HZ_SHADOWS_GLSL

#include "../core/defines.glsl"
#include "../core/uniforms.glsl"
#include "../core/math.glsl"

// =============================================================================
// SHADOW MAP SAMPLERS
// =============================================================================

// Main shadow map (for single shadow or CSM layer 0)
uniform sampler2D hz_ShadowMap;

// CSM array (if using texture array)
// uniform sampler2DArray hz_ShadowMapArray;

// =============================================================================
// BASIC SHADOW
// =============================================================================

float SampleShadow(vec3 shadowCoord, float bias) {
    float depth = texture(hz_ShadowMap, shadowCoord.xy).r;
    return shadowCoord.z - bias > depth ? 1.0 : 0.0;
}

// =============================================================================
// PCF (Percentage Closer Filtering)
// =============================================================================

float PCF_2x2(vec3 shadowCoord, float bias, vec2 texelSize) {
    float shadow = 0.0;
    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            vec2 offset = vec2(x, y) * texelSize;
            float depth = texture(hz_ShadowMap, shadowCoord.xy + offset).r;
            shadow += shadowCoord.z - bias > depth ? 1.0 : 0.0;
        }
    }
    return shadow / 9.0;
}

float PCF_3x3(vec3 shadowCoord, float bias, vec2 texelSize) {
    float shadow = 0.0;
    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            vec2 offset = vec2(x, y) * texelSize;
            float depth = texture(hz_ShadowMap, shadowCoord.xy + offset).r;
            shadow += shadowCoord.z - bias > depth ? 1.0 : 0.0;
        }
    }
    return shadow / 9.0;
}

float PCF_Poisson(vec3 shadowCoord, float bias, vec2 texelSize, float spreadRadius) {
    float shadow = 0.0;
    for (int i = 0; i < PCF_SAMPLES; i++) {
        vec2 offset = poissonDisk[i] * spreadRadius * texelSize;
        float depth = texture(hz_ShadowMap, shadowCoord.xy + offset).r;
        shadow += shadowCoord.z - bias > depth ? 1.0 : 0.0;
    }
    return shadow / float(PCF_SAMPLES);
}

// Rotated Poisson for noise reduction
float PCF_RotatedPoisson(vec3 shadowCoord, float bias, vec2 texelSize, float spreadRadius, float rotation) {
    float shadow = 0.0;
    float c = cos(rotation);
    float s = sin(rotation);
    mat2 rotMat = mat2(c, -s, s, c);
    
    for (int i = 0; i < PCF_SAMPLES; i++) {
        vec2 offset = rotMat * poissonDisk[i] * spreadRadius * texelSize;
        float depth = texture(hz_ShadowMap, shadowCoord.xy + offset).r;
        shadow += shadowCoord.z - bias > depth ? 1.0 : 0.0;
    }
    return shadow / float(PCF_SAMPLES);
}

// =============================================================================
// PCSS (Percentage Closer Soft Shadows)
// =============================================================================

// Search for blockers
float PCSS_BlockerSearch(vec3 shadowCoord, float bias, vec2 texelSize, float lightSize) {
    float blockerSum = 0.0;
    float numBlockers = 0.0;
    float searchRadius = lightSize * shadowCoord.z;
    
    for (int i = 0; i < PCF_SAMPLES; i++) {
        vec2 offset = poissonDisk[i] * searchRadius * texelSize;
        float depth = texture(hz_ShadowMap, shadowCoord.xy + offset).r;
        if (depth < shadowCoord.z - bias) {
            blockerSum += depth;
            numBlockers += 1.0;
        }
    }
    
    return numBlockers > 0.0 ? blockerSum / numBlockers : -1.0;
}

// Full PCSS
float ShadowPCSS(vec3 shadowCoord, float bias, vec2 texelSize, float lightSize) {
    // Step 1: Blocker search
    float avgBlockerDepth = PCSS_BlockerSearch(shadowCoord, bias, texelSize, lightSize);
    
    if (avgBlockerDepth < 0.0) {
        return 0.0;  // No blockers
    }
    
    // Step 2: Penumbra estimation
    float penumbraRatio = (shadowCoord.z - avgBlockerDepth) / avgBlockerDepth;
    float filterRadius = penumbraRatio * lightSize;
    
    // Step 3: PCF with variable radius
    return PCF_Poisson(shadowCoord, bias, texelSize, filterRadius);
}

// =============================================================================
// VSM (Variance Shadow Maps)
// =============================================================================

float linstep(float low, float high, float v) {
    return clamp((v - low) / (high - low), 0.0, 1.0);
}

float ShadowVSM(vec3 shadowCoord, float minVariance, float lightBleed) {
    vec2 moments = texture(hz_ShadowMap, shadowCoord.xy).rg;
    
    float p = step(shadowCoord.z, moments.x);
    float variance = max(moments.y - moments.x * moments.x, minVariance);
    
    float d = shadowCoord.z - moments.x;
    float pMax = linstep(lightBleed, 1.0, variance / (variance + d * d));
    
    return min(max(p, pMax), 1.0);
}

// =============================================================================
// ESM (Exponential Shadow Maps)
// =============================================================================

float ShadowESM(vec3 shadowCoord, float exponent) {
    float occluder = texture(hz_ShadowMap, shadowCoord.xy).r;
    return clamp(exp(-exponent * (shadowCoord.z - occluder)), 0.0, 1.0);
}

// =============================================================================
// CASCADED SHADOW MAPS
// =============================================================================

// Find which cascade to use
int FindCascade(float viewDepth) {
    for (int i = 0; i < int(hz_Shadow.hz_ShadowParams.w); i++) {
        if (viewDepth < hz_Shadow.hz_CascadeSplits[i]) {
            return i;
        }
    }
    return int(hz_Shadow.hz_ShadowParams.w) - 1;
}

// Get shadow coordinate for cascade
vec3 GetCascadeShadowCoord(vec3 worldPos, int cascade) {
    vec4 shadowCoord = hz_Shadow.hz_LightSpaceMatrices[cascade] * vec4(worldPos, 1.0);
    shadowCoord.xyz /= shadowCoord.w;
    shadowCoord.xyz = shadowCoord.xyz * 0.5 + 0.5;
    return shadowCoord.xyz;
}

// CSM with cascade blending
float ShadowCSM(vec3 worldPos, vec3 N, vec3 L, float viewDepth) {
    int cascade = FindCascade(viewDepth);
    vec3 shadowCoord = GetCascadeShadowCoord(worldPos, cascade);
    
    // Early out for outside shadow range
    if (shadowCoord.z > 1.0) return 0.0;
    
    // Calculate bias
    float baseBias = hz_Shadow.hz_ShadowParams.x;
    float normalBias = hz_Shadow.hz_ShadowParams.y;
    float bias = max(baseBias * (1.0 - dot(N, L)), baseBias * 0.1);
    
    // Sample shadow
    vec2 texelSize = hz_Shadow.hz_ShadowMapSize.zw;
    float shadow = PCF_3x3(shadowCoord, bias, texelSize);
    
    // Cascade blending
    float blendDistance = hz_Shadow.hz_ShadowParams.z;
    float splitDistance = hz_Shadow.hz_CascadeSplits[cascade];
    float fadeFactor = (splitDistance - viewDepth) / blendDistance;
    
    if (fadeFactor < 1.0 && cascade < int(hz_Shadow.hz_ShadowParams.w) - 1) {
        vec3 nextShadowCoord = GetCascadeShadowCoord(worldPos, cascade + 1);
        float nextShadow = PCF_3x3(nextShadowCoord, bias, texelSize);
        shadow = mix(nextShadow, shadow, saturate(fadeFactor));
    }
    
    return shadow;
}

// =============================================================================
// CONTACT SHADOWS (Screen-space)
// =============================================================================

float ContactShadow(
    vec3 rayOrigin,
    vec3 rayDir,
    sampler2D depthBuffer,
    mat4 viewProj,
    int steps,
    float maxDistance
) {
    float stepSize = maxDistance / float(steps);
    vec3 rayPos = rayOrigin;
    
    for (int i = 0; i < steps; i++) {
        rayPos += rayDir * stepSize;
        
        vec4 projPos = viewProj * vec4(rayPos, 1.0);
        projPos.xyz /= projPos.w;
        projPos.xy = projPos.xy * 0.5 + 0.5;
        
        if (projPos.x < 0.0 || projPos.x > 1.0 || projPos.y < 0.0 || projPos.y > 1.0) {
            break;
        }
        
        float sceneDepth = texture(depthBuffer, projPos.xy).r;
        float rayDepth = projPos.z;
        
        if (rayDepth > sceneDepth && rayDepth - sceneDepth < 0.01) {
            return 1.0;  // In shadow
        }
    }
    
    return 0.0;  // Not in shadow
}

// =============================================================================
// UTILITY
// =============================================================================

// Normal offset bias (prevents shadow acne on surfaces)
vec3 GetShadowPosition(vec3 worldPos, vec3 N, float bias) {
    return worldPos + N * bias;
}

// Slope-scaled bias
float GetSlopeBias(vec3 N, vec3 L, float baseBias) {
    float cosAngle = saturate(dot(N, L));
    float sinAngle = sqrt(1.0 - cosAngle * cosAngle);
    float tanAngle = sinAngle / max(cosAngle, EPSILON);
    return baseBias + baseBias * clamp(tanAngle, 0.0, 1.0);
}

// Debug cascade visualization
vec3 DebugCascadeColor(int cascade) {
    const vec3 colors[4] = vec3[](
        vec3(1.0, 0.0, 0.0),  // Red
        vec3(0.0, 1.0, 0.0),  // Green
        vec3(0.0, 0.0, 1.0),  // Blue
        vec3(1.0, 1.0, 0.0)   // Yellow
    );
    return colors[min(cascade, 3)];
}

#endif // HZ_SHADOWS_GLSL
