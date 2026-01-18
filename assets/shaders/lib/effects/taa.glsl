/**
 * =============================================================================
 * HORIZON ENGINE - SHADER LIBRARY v2.0
 * =============================================================================
 * 
 * File: effects/taa.glsl
 * Purpose: Temporal Anti-Aliasing
 * =============================================================================
 */

#ifndef HZ_TAA_GLSL
#define HZ_TAA_GLSL

#include "../core/defines.glsl"
#include "../core/uniforms.glsl"
#include "../core/color.glsl"

// =============================================================================
// TAA UTILITIES
// =============================================================================

// RGB to YCoCg (better for neighborhood clamping)
vec3 RGBToYCoCg(vec3 rgb) {
    float Co = rgb.r - rgb.b;
    float t = rgb.b + Co * 0.5;
    float Cg = rgb.g - t;
    float Y = t + Cg * 0.5;
    return vec3(Y, Co, Cg);
}

vec3 YCoCgToRGB(vec3 ycocg) {
    float t = ycocg.x - ycocg.z * 0.5;
    float g = ycocg.z + t;
    float b = t - ycocg.y * 0.5;
    float r = ycocg.y + b;
    return vec3(r, g, b);
}

// Clip to AABB (Playdead technique)
vec3 ClipToAABB(vec3 color, vec3 minBound, vec3 maxBound) {
    vec3 center = (minBound + maxBound) * 0.5;
    vec3 extents = (maxBound - minBound) * 0.5;
    
    vec3 offset = color - center;
    vec3 ts = abs(extents) / (abs(offset) + EPSILON);
    float t = saturate(min(min(ts.x, ts.y), ts.z));
    
    return center + offset * t;
}

// Variance clipping
vec3 VarianceClipping(vec3 color, vec3 mean, vec3 stdDev, float gamma) {
    vec3 minBound = mean - gamma * stdDev;
    vec3 maxBound = mean + gamma * stdDev;
    return clamp(color, minBound, maxBound);
}

// =============================================================================
// NEIGHBORHOOD SAMPLING
// =============================================================================

struct Neighborhood {
    vec3 min_;
    vec3 max_;
    vec3 mean;
    vec3 stdDev;
};

Neighborhood SampleNeighborhood3x3(sampler2D tex, vec2 uv, vec2 texelSize) {
    vec3 samples[9];
    vec3 sum = vec3(0.0);
    vec3 sumSq = vec3(0.0);
    
    int idx = 0;
    for (int y = -1; y <= 1; y++) {
        for (int x = -1; x <= 1; x++) {
            vec3 color = texture(tex, uv + vec2(x, y) * texelSize).rgb;
            samples[idx] = RGBToYCoCg(color);
            sum += samples[idx];
            sumSq += samples[idx] * samples[idx];
            idx++;
        }
    }
    
    Neighborhood n;
    n.mean = sum / 9.0;
    n.stdDev = sqrt(abs(sumSq / 9.0 - n.mean * n.mean));
    
    // Min/Max
    n.min_ = samples[0];
    n.max_ = samples[0];
    for (int i = 1; i < 9; i++) {
        n.min_ = min(n.min_, samples[i]);
        n.max_ = max(n.max_, samples[i]);
    }
    
    return n;
}

// Plus pattern (faster, less ghosting)
Neighborhood SampleNeighborhoodPlus(sampler2D tex, vec2 uv, vec2 texelSize) {
    vec3 center = RGBToYCoCg(texture(tex, uv).rgb);
    vec3 left = RGBToYCoCg(texture(tex, uv - vec2(texelSize.x, 0.0)).rgb);
    vec3 right = RGBToYCoCg(texture(tex, uv + vec2(texelSize.x, 0.0)).rgb);
    vec3 up = RGBToYCoCg(texture(tex, uv + vec2(0.0, texelSize.y)).rgb);
    vec3 down = RGBToYCoCg(texture(tex, uv - vec2(0.0, texelSize.y)).rgb);
    
    Neighborhood n;
    n.min_ = min(min(min(min(center, left), right), up), down);
    n.max_ = max(max(max(max(center, left), right), up), down);
    n.mean = (center + left + right + up + down) / 5.0;
    
    vec3 sumSq = center * center + left * left + right * right + up * up + down * down;
    n.stdDev = sqrt(abs(sumSq / 5.0 - n.mean * n.mean));
    
    return n;
}

// =============================================================================
// MOTION VECTORS
// =============================================================================

vec2 GetMotionVector(vec2 uv, sampler2D velocityTex) {
    return texture(velocityTex, uv).rg;
}

vec2 CalculateMotionVector(vec3 worldPos, mat4 prevViewProj, vec2 uv) {
    vec4 prevClip = prevViewProj * vec4(worldPos, 1.0);
    vec2 prevUV = (prevClip.xy / prevClip.w) * 0.5 + 0.5;
    return uv - prevUV;
}

// Find closest depth in 3x3 neighborhood (for sharper edges)
vec2 GetClosestVelocity(sampler2D depthTex, sampler2D velocityTex, vec2 uv, vec2 texelSize) {
    float closestDepth = 0.0;
    vec2 closestUV = uv;
    
    for (int y = -1; y <= 1; y++) {
        for (int x = -1; x <= 1; x++) {
            vec2 sampleUV = uv + vec2(x, y) * texelSize;
            float depth = texture(depthTex, sampleUV).r;
            if (depth > closestDepth) {
                closestDepth = depth;
                closestUV = sampleUV;
            }
        }
    }
    
    return texture(velocityTex, closestUV).rg;
}

// =============================================================================
// TEMPORAL ACCUMULATION
// =============================================================================

// Calculate blend factor based on velocity
float CalculateFeedback(vec2 velocity, float minFeedback, float maxFeedback) {
    float velocityMag = length(velocity);
    float velocityWeight = saturate(velocityMag * 20.0);  // Tune this
    return mix(maxFeedback, minFeedback, velocityWeight);
}

// Luminance-based feedback (reduces ghosting on high contrast)
float CalculateLuminanceFeedback(vec3 current, vec3 history, float baseFeedback) {
    float lumCurrent = luminance(current);
    float lumHistory = luminance(history);
    float lumDiff = abs(lumCurrent - lumHistory);
    float lumWeight = saturate(lumDiff * 4.0);
    return mix(baseFeedback, 0.8, lumWeight);
}

// =============================================================================
// FULL TAA RESOLVE
// =============================================================================

vec3 TAA_Resolve(
    sampler2D currentTex,
    sampler2D historyTex,
    sampler2D velocityTex,
    sampler2D depthTex,
    vec2 uv,
    vec2 texelSize,
    float feedbackMin,
    float feedbackMax
) {
    // Get velocity from closest depth neighbor
    vec2 velocity = GetClosestVelocity(depthTex, velocityTex, uv, texelSize);
    vec2 historyUV = uv - velocity;
    
    // Sample current and history
    vec3 current = texture(currentTex, uv).rgb;
    vec3 history = texture(historyTex, historyUV).rgb;
    
    // Check if history is valid
    if (historyUV.x < 0.0 || historyUV.x > 1.0 || historyUV.y < 0.0 || historyUV.y > 1.0) {
        return current;
    }
    
    // Sample neighborhood and clip history
    Neighborhood n = SampleNeighborhood3x3(currentTex, uv, texelSize);
    
    vec3 historyYCoCg = RGBToYCoCg(history);
    
    // Variance clipping (AABB in YCoCg space)
    const float gamma = 1.25;  // Tune for less/more ghosting
    vec3 clippedHistory = ClipToAABB(historyYCoCg, 
                                      n.mean - gamma * n.stdDev,
                                      n.mean + gamma * n.stdDev);
    history = YCoCgToRGB(clippedHistory);
    
    // Calculate feedback
    float feedback = CalculateFeedback(velocity, feedbackMin, feedbackMax);
    feedback = CalculateLuminanceFeedback(current, history, feedback);
    
    // Blend
    vec3 result = mix(current, history, feedback);
    
    // Anti-flicker (optional)
    float currentWeight = 1.0 - feedback;
    float historyWeight = feedback;
    float weightSum = currentWeight + historyWeight;
    
    return result;
}

// =============================================================================
// TAA SHARPENING (Post-resolve)
// =============================================================================

vec3 TAA_Sharpen(sampler2D tex, vec2 uv, vec2 texelSize, float strength) {
    vec3 center = texture(tex, uv).rgb;
    vec3 top = texture(tex, uv + vec2(0.0, texelSize.y)).rgb;
    vec3 bottom = texture(tex, uv - vec2(0.0, texelSize.y)).rgb;
    vec3 left = texture(tex, uv - vec2(texelSize.x, 0.0)).rgb;
    vec3 right = texture(tex, uv + vec2(texelSize.x, 0.0)).rgb;
    
    vec3 neighbors = (top + bottom + left + right) * 0.25;
    vec3 sharpened = center + (center - neighbors) * strength;
    
    // Clamp to prevent ringing
    vec3 minVal = min(min(min(min(center, top), bottom), left), right);
    vec3 maxVal = max(max(max(max(center, top), bottom), left), right);
    
    return clamp(sharpened, minVal, maxVal);
}

#endif // HZ_TAA_GLSL
