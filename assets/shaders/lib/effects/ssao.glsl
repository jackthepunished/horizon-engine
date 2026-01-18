/**
 * =============================================================================
 * HORIZON ENGINE - SHADER LIBRARY v2.0
 * =============================================================================
 * 
 * File: effects/ssao.glsl
 * Purpose: Screen-Space Ambient Occlusion
 * =============================================================================
 */

#ifndef HZ_SSAO_GLSL
#define HZ_SSAO_GLSL

#include "../core/defines.glsl"
#include "../core/uniforms.glsl"
#include "../core/math.glsl"

// =============================================================================
// SSAO PARAMETERS
// =============================================================================

// Kernel samples (hemisphere)
uniform vec3 hz_SSAOKernel[SSAO_KERNEL_SIZE];
uniform sampler2D hz_NoiseTexture;
uniform float hz_SSAORadius;
uniform float hz_SSAOBias;
uniform float hz_SSAOIntensity;

// =============================================================================
// BASIC SSAO (Crytek-style)
// =============================================================================

float SSAO_Crytek(
    sampler2D depthTex,
    sampler2D normalTex,
    vec2 uv,
    vec2 noiseScale
) {
    float depth = texture(depthTex, uv).r;
    if (depth >= 1.0) return 1.0;
    
    vec3 fragPos = hz_ReconstructWorldPos(uv, depth);
    vec3 normal = decodeOctahedron(texture(normalTex, uv).rg);
    vec3 randomVec = normalize(texture(hz_NoiseTexture, uv * noiseScale).xyz * 2.0 - 1.0);
    
    // Create TBN matrix
    vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN = mat3(tangent, bitangent, normal);
    
    float occlusion = 0.0;
    for (int i = 0; i < SSAO_KERNEL_SIZE; i++) {
        // Sample position in view space
        vec3 samplePos = fragPos + TBN * hz_SSAOKernel[i] * hz_SSAORadius;
        
        // Project sample to screen
        vec4 offset = hz_Camera.hz_ViewProjection * vec4(samplePos, 1.0);
        offset.xyz /= offset.w;
        offset.xyz = offset.xyz * 0.5 + 0.5;
        
        // Sample depth at offset
        float sampleDepth = texture(depthTex, offset.xy).r;
        vec3 sampleWorldPos = hz_ReconstructWorldPos(offset.xy, sampleDepth);
        
        // Range check
        float rangeCheck = smoothstep(0.0, 1.0, hz_SSAORadius / abs(fragPos.z - sampleWorldPos.z));
        
        // Occlusion contribution
        occlusion += (sampleWorldPos.z >= samplePos.z + hz_SSAOBias ? 1.0 : 0.0) * rangeCheck;
    }
    
    occlusion = 1.0 - (occlusion / float(SSAO_KERNEL_SIZE));
    return pow(occlusion, hz_SSAOIntensity);
}

// =============================================================================
// HBAO (Horizon-Based Ambient Occlusion)
// =============================================================================

float HBAO(
    sampler2D depthTex,
    vec2 uv,
    vec2 texelSize,
    int numDirections,
    int numSteps,
    float radius,
    float intensity
) {
    float depth = texture(depthTex, uv).r;
    if (depth >= 1.0) return 1.0;
    
    vec3 viewPos = hz_ReconstructWorldPos(uv, depth);
    
    // Reconstruct normal from depth
    float depthL = texture(depthTex, uv - vec2(texelSize.x, 0.0)).r;
    float depthR = texture(depthTex, uv + vec2(texelSize.x, 0.0)).r;
    float depthU = texture(depthTex, uv + vec2(0.0, texelSize.y)).r;
    float depthD = texture(depthTex, uv - vec2(0.0, texelSize.y)).r;
    
    vec3 posL = hz_ReconstructWorldPos(uv - vec2(texelSize.x, 0.0), depthL);
    vec3 posR = hz_ReconstructWorldPos(uv + vec2(texelSize.x, 0.0), depthR);
    vec3 posU = hz_ReconstructWorldPos(uv + vec2(0.0, texelSize.y), depthU);
    vec3 posD = hz_ReconstructWorldPos(uv - vec2(0.0, texelSize.y), depthD);
    
    vec3 normal = normalize(cross(posR - posL, posU - posD));
    
    float occlusion = 0.0;
    float radiusPixels = radius / viewPos.z;
    
    for (int d = 0; d < numDirections; d++) {
        float angle = float(d) * TWO_PI / float(numDirections);
        vec2 direction = vec2(cos(angle), sin(angle));
        
        float maxHorizon = -1.0;
        
        for (int s = 1; s <= numSteps; s++) {
            vec2 offset = direction * radiusPixels * float(s) / float(numSteps) * texelSize;
            vec2 sampleUV = uv + offset;
            
            float sampleDepth = texture(depthTex, sampleUV).r;
            vec3 samplePos = hz_ReconstructWorldPos(sampleUV, sampleDepth);
            
            vec3 horizon = samplePos - viewPos;
            float horizonAngle = dot(normalize(horizon), normal);
            
            maxHorizon = max(maxHorizon, horizonAngle);
        }
        
        occlusion += saturate(maxHorizon);
    }
    
    occlusion /= float(numDirections);
    return pow(1.0 - occlusion, intensity);
}

// =============================================================================
// GTAO (Ground Truth Ambient Occlusion)
// =============================================================================

float GTAO(
    sampler2D depthTex,
    vec2 uv,
    vec2 texelSize,
    int slices,
    int stepsPerSlice,
    float radius,
    float falloffStart,
    float falloffEnd
) {
    float depth = texture(depthTex, uv).r;
    if (depth >= 1.0) return 1.0;
    
    vec3 viewPos = hz_ReconstructWorldPos(uv, depth);
    
    // Reconstruct normal
    float depthL = texture(depthTex, uv - vec2(texelSize.x, 0.0)).r;
    float depthR = texture(depthTex, uv + vec2(texelSize.x, 0.0)).r;
    float depthU = texture(depthTex, uv + vec2(0.0, texelSize.y)).r;
    float depthD = texture(depthTex, uv - vec2(0.0, texelSize.y)).r;
    
    vec3 posL = hz_ReconstructWorldPos(uv - vec2(texelSize.x, 0.0), depthL);
    vec3 posR = hz_ReconstructWorldPos(uv + vec2(texelSize.x, 0.0), depthR);
    vec3 posU = hz_ReconstructWorldPos(uv + vec2(0.0, texelSize.y), depthU);
    vec3 posD = hz_ReconstructWorldPos(uv - vec2(0.0, texelSize.y), depthD);
    
    vec3 normal = normalize(cross(posR - posL, posU - posD));
    
    float visibility = 0.0;
    float radiusPixels = radius / abs(viewPos.z);
    
    // Random rotation
    float rotationAngle = hash12(uv * 1000.0 + hz_Scene.hz_TimeParams.x) * TWO_PI;
    
    for (int slice = 0; slice < slices; slice++) {
        float sliceAngle = (float(slice) / float(slices)) * PI + rotationAngle;
        vec2 direction = vec2(cos(sliceAngle), sin(sliceAngle));
        
        // Find horizon in both directions
        float horizonCos1 = -1.0;
        float horizonCos2 = -1.0;
        
        for (int step = 1; step <= stepsPerSlice; step++) {
            float t = float(step) / float(stepsPerSlice);
            vec2 offset = direction * radiusPixels * t * texelSize;
            
            // Positive direction
            vec2 sampleUV1 = uv + offset;
            float sampleDepth1 = texture(depthTex, sampleUV1).r;
            vec3 samplePos1 = hz_ReconstructWorldPos(sampleUV1, sampleDepth1);
            vec3 diff1 = samplePos1 - viewPos;
            float dist1 = length(diff1);
            float falloff1 = saturate((falloffEnd - dist1) / (falloffEnd - falloffStart));
            horizonCos1 = max(horizonCos1, dot(normalize(diff1), normal) * falloff1);
            
            // Negative direction
            vec2 sampleUV2 = uv - offset;
            float sampleDepth2 = texture(depthTex, sampleUV2).r;
            vec3 samplePos2 = hz_ReconstructWorldPos(sampleUV2, sampleDepth2);
            vec3 diff2 = samplePos2 - viewPos;
            float dist2 = length(diff2);
            float falloff2 = saturate((falloffEnd - dist2) / (falloffEnd - falloffStart));
            horizonCos2 = max(horizonCos2, dot(normalize(diff2), normal) * falloff2);
        }
        
        // Integrate visibility
        float h1 = acos(clamp(horizonCos1, -1.0, 1.0));
        float h2 = acos(clamp(horizonCos2, -1.0, 1.0));
        visibility += (sin(h1) + sin(h2)) / 2.0;
    }
    
    return visibility / float(slices);
}

// =============================================================================
// SSAO BLUR (Bilateral)
// =============================================================================

float SSAOBilateralBlur(sampler2D aoTex, sampler2D depthTex, vec2 uv, vec2 texelSize, int radius) {
    float centerAO = texture(aoTex, uv).r;
    float centerDepth = texture(depthTex, uv).r;
    
    float totalAO = 0.0;
    float totalWeight = 0.0;
    
    for (int x = -radius; x <= radius; x++) {
        for (int y = -radius; y <= radius; y++) {
            vec2 offset = vec2(x, y) * texelSize;
            vec2 sampleUV = uv + offset;
            
            float sampleAO = texture(aoTex, sampleUV).r;
            float sampleDepth = texture(depthTex, sampleUV).r;
            
            // Bilateral weight
            float spatialWeight = exp(-float(x * x + y * y) / (2.0 * float(radius * radius)));
            float depthWeight = exp(-abs(centerDepth - sampleDepth) * 1000.0);
            float weight = spatialWeight * depthWeight;
            
            totalAO += sampleAO * weight;
            totalWeight += weight;
        }
    }
    
    return totalAO / totalWeight;
}

#endif // HZ_SSAO_GLSL
