/**
 * =============================================================================
 * HORIZON ENGINE - SHADER LIBRARY v2.0
 * =============================================================================
 * 
 * File: effects/postprocess.glsl
 * Purpose: Post-processing effects (bloom, DOF, motion blur, etc.)
 * =============================================================================
 */

#ifndef HZ_POSTPROCESS_GLSL
#define HZ_POSTPROCESS_GLSL

#include "../core/defines.glsl"
#include "../core/color.glsl"

// =============================================================================
// BLOOM
// =============================================================================

// Karis average for bloom threshold (reduces fireflies)
vec3 KarisAverage(vec3 c) {
    float luma = luminance(c);
    return c / (1.0 + luma);
}

// Bloom threshold with soft knee
vec3 BloomThreshold(vec3 color, float threshold, float knee) {
    float brightness = luminance(color);
    float soft = brightness - threshold + knee;
    soft = clamp(soft, 0.0, 2.0 * knee);
    soft = soft * soft / (4.0 * knee + EPSILON);
    float contribution = max(soft, brightness - threshold);
    contribution /= max(brightness, EPSILON);
    return color * contribution;
}

// Dual filter downsampling (Kawase-like)
vec3 DownsampleBox13Tap(sampler2D tex, vec2 uv, vec2 texelSize) {
    vec3 A = texture(tex, uv + texelSize * vec2(-1.0, -1.0)).rgb;
    vec3 B = texture(tex, uv + texelSize * vec2( 0.0, -1.0)).rgb;
    vec3 C = texture(tex, uv + texelSize * vec2( 1.0, -1.0)).rgb;
    vec3 D = texture(tex, uv + texelSize * vec2(-0.5, -0.5)).rgb;
    vec3 E = texture(tex, uv + texelSize * vec2( 0.5, -0.5)).rgb;
    vec3 F = texture(tex, uv + texelSize * vec2(-1.0,  0.0)).rgb;
    vec3 G = texture(tex, uv).rgb;
    vec3 H = texture(tex, uv + texelSize * vec2( 1.0,  0.0)).rgb;
    vec3 I = texture(tex, uv + texelSize * vec2(-0.5,  0.5)).rgb;
    vec3 J = texture(tex, uv + texelSize * vec2( 0.5,  0.5)).rgb;
    vec3 K = texture(tex, uv + texelSize * vec2(-1.0,  1.0)).rgb;
    vec3 L = texture(tex, uv + texelSize * vec2( 0.0,  1.0)).rgb;
    vec3 M = texture(tex, uv + texelSize * vec2( 1.0,  1.0)).rgb;
    
    vec3 result = vec3(0.0);
    result += (D + E + I + J) * 0.5;
    result += (A + B + G + F) * 0.125;
    result += (B + C + H + G) * 0.125;
    result += (F + G + L + K) * 0.125;
    result += (G + H + M + L) * 0.125;
    return result * 0.25;
}

// Tent filter upsampling
vec3 UpsampleTent(sampler2D tex, vec2 uv, vec2 texelSize) {
    vec3 d = texelSize.xyx * vec3(1.0, 1.0, 0.0);
    
    vec3 s;
    s  = texture(tex, uv - d.xy).rgb;
    s += texture(tex, uv - d.zy).rgb * 2.0;
    s += texture(tex, uv - d.xy * vec2(-1, 1)).rgb;
    s += texture(tex, uv + d.zx * vec2(-1, 0)).rgb * 2.0;
    s += texture(tex, uv).rgb * 4.0;
    s += texture(tex, uv + d.zx).rgb * 2.0;
    s += texture(tex, uv + d.xy * vec2(-1, 1)).rgb;
    s += texture(tex, uv + d.zy).rgb * 2.0;
    s += texture(tex, uv + d.xy).rgb;
    
    return s / 16.0;
}

// =============================================================================
// GAUSSIAN BLUR
// =============================================================================

// 5-tap Gaussian weights
const float gaussianWeights5[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);

vec3 GaussianBlur5(sampler2D tex, vec2 uv, vec2 direction) {
    vec3 result = texture(tex, uv).rgb * gaussianWeights5[0];
    for (int i = 1; i < 5; i++) {
        result += texture(tex, uv + direction * float(i)).rgb * gaussianWeights5[i];
        result += texture(tex, uv - direction * float(i)).rgb * gaussianWeights5[i];
    }
    return result;
}

// 9-tap Gaussian
const float gaussianWeights9[9] = float[](
    0.0625, 0.09375, 0.125, 0.140625, 0.15625,
    0.140625, 0.125, 0.09375, 0.0625
);

vec3 GaussianBlur9(sampler2D tex, vec2 uv, vec2 texelSize, bool horizontal) {
    vec2 direction = horizontal ? vec2(texelSize.x, 0.0) : vec2(0.0, texelSize.y);
    
    vec3 result = vec3(0.0);
    for (int i = -4; i <= 4; i++) {
        result += texture(tex, uv + direction * float(i)).rgb * gaussianWeights9[i + 4];
    }
    return result;
}

// =============================================================================
// DEPTH OF FIELD
// =============================================================================

// Circle of confusion
float CalculateCoC(float depth, float focusDistance, float focalLength, float aperture) {
    return abs(aperture * (focalLength * (focusDistance - depth)) / 
               (depth * (focusDistance - focalLength)));
}

// Simple bokeh DOF (gather-based)
vec3 DOF_Gather(sampler2D colorTex, sampler2D depthTex, vec2 uv, vec2 texelSize,
                float focusDistance, float focusRange, float maxBlur) {
    float depth = texture(depthTex, uv).r;
    float blur = saturate(abs(depth - focusDistance) / focusRange);
    blur *= maxBlur;
    
    vec3 color = vec3(0.0);
    float totalWeight = 0.0;
    int samples = 16;
    
    for (int i = 0; i < samples; i++) {
        vec2 offset = poissonDisk[i] * blur * texelSize;
        float sampleDepth = texture(depthTex, uv + offset).r;
        float sampleBlur = saturate(abs(sampleDepth - focusDistance) / focusRange);
        
        // Only sample from further or equally blurred pixels
        float weight = sampleBlur >= blur ? 1.0 : 0.0;
        color += texture(colorTex, uv + offset).rgb * weight;
        totalWeight += weight;
    }
    
    return totalWeight > 0.0 ? color / totalWeight : texture(colorTex, uv).rgb;
}

// =============================================================================
// MOTION BLUR
// =============================================================================

// Per-pixel motion blur
vec3 MotionBlur(sampler2D colorTex, sampler2D velocityTex, vec2 uv, int samples, float intensity) {
    vec2 velocity = texture(velocityTex, uv).rg * intensity;
    
    vec3 color = texture(colorTex, uv).rgb;
    float totalWeight = 1.0;
    
    for (int i = 1; i < samples; i++) {
        float t = float(i) / float(samples - 1) - 0.5;
        vec2 offset = velocity * t;
        
        color += texture(colorTex, uv + offset).rgb;
        totalWeight += 1.0;
    }
    
    return color / totalWeight;
}

// Camera motion blur (object-based)
vec3 CameraMotionBlur(sampler2D colorTex, sampler2D depthTex, vec2 uv, 
                       mat4 viewProjInv, mat4 prevViewProj, int samples) {
    float depth = texture(depthTex, uv).r;
    
    // Reconstruct world position
    vec4 clipPos = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 worldPos = viewProjInv * clipPos;
    worldPos /= worldPos.w;
    
    // Project to previous frame
    vec4 prevClipPos = prevViewProj * worldPos;
    prevClipPos /= prevClipPos.w;
    vec2 prevUV = prevClipPos.xy * 0.5 + 0.5;
    
    vec2 velocity = uv - prevUV;
    
    vec3 color = vec3(0.0);
    for (int i = 0; i < samples; i++) {
        float t = float(i) / float(samples - 1);
        vec2 sampleUV = mix(uv, prevUV, t);
        color += texture(colorTex, sampleUV).rgb;
    }
    
    return color / float(samples);
}

// =============================================================================
// CHROMATIC ABERRATION
// =============================================================================

vec3 ChromaticAberration(sampler2D tex, vec2 uv, float intensity) {
    vec2 direction = uv - 0.5;
    vec2 offset = direction * intensity;
    
    vec3 color;
    color.r = texture(tex, uv + offset).r;
    color.g = texture(tex, uv).g;
    color.b = texture(tex, uv - offset).b;
    
    return color;
}

// =============================================================================
// FILM GRAIN
// =============================================================================

float FilmGrain(vec2 uv, float time, float intensity) {
    vec2 p = uv + vec2(time, 0.0);
    float grain = hash12(p * 1000.0);
    return (grain - 0.5) * intensity;
}

vec3 ApplyFilmGrain(vec3 color, vec2 uv, float time, float intensity) {
    float grain = FilmGrain(uv, time, intensity);
    // Apply more grain to darker areas (photographic)
    float luminanceVal = luminance(color);
    float grainAmount = grain * (1.0 - luminanceVal * 0.5);
    return color + grainAmount;
}

// =============================================================================
// LENS EFFECTS
// =============================================================================

// Simple lens dirt (multiply with bloom)
vec3 LensDirt(vec3 bloom, sampler2D dirtTex, vec2 uv, float intensity) {
    vec3 dirt = texture(dirtTex, uv).rgb;
    return bloom + bloom * dirt * intensity;
}

// Screen-space lens flare ghosts
vec3 LensFlareGhosts(sampler2D tex, vec2 uv, int ghostCount, float ghostDispersal, float distortion) {
    vec3 result = vec3(0.0);
    vec2 ghostVec = (0.5 - uv) * ghostDispersal;
    
    for (int i = 0; i < ghostCount; i++) {
        vec2 offset = fract(uv + ghostVec * float(i));
        float weight = length(0.5 - offset) / length(vec2(0.5));
        weight = pow(1.0 - weight, 10.0);
        
        // Chromatic distortion
        vec3 sample_;
        sample_.r = texture(tex, offset + ghostVec * distortion).r;
        sample_.g = texture(tex, offset).g;
        sample_.b = texture(tex, offset - ghostVec * distortion).b;
        
        result += sample_ * weight;
    }
    
    return result;
}

// =============================================================================
// SHARPEN
// =============================================================================

vec3 Sharpen(sampler2D tex, vec2 uv, vec2 texelSize, float strength) {
    vec3 center = texture(tex, uv).rgb;
    vec3 up = texture(tex, uv + vec2(0.0, texelSize.y)).rgb;
    vec3 down = texture(tex, uv - vec2(0.0, texelSize.y)).rgb;
    vec3 left = texture(tex, uv - vec2(texelSize.x, 0.0)).rgb;
    vec3 right = texture(tex, uv + vec2(texelSize.x, 0.0)).rgb;
    
    vec3 sharpened = center * (1.0 + 4.0 * strength) - (up + down + left + right) * strength;
    return max(sharpened, vec3(0.0));
}

// CAS (Contrast Adaptive Sharpening) - AMD FidelityFX
vec3 ContrastAdaptiveSharpen(sampler2D tex, vec2 uv, vec2 texelSize, float sharpness) {
    // Fetch 3x3 neighborhood
    vec3 a = texture(tex, uv + texelSize * vec2(-1, -1)).rgb;
    vec3 b = texture(tex, uv + texelSize * vec2( 0, -1)).rgb;
    vec3 c = texture(tex, uv + texelSize * vec2( 1, -1)).rgb;
    vec3 d = texture(tex, uv + texelSize * vec2(-1,  0)).rgb;
    vec3 e = texture(tex, uv).rgb;
    vec3 f = texture(tex, uv + texelSize * vec2( 1,  0)).rgb;
    vec3 g = texture(tex, uv + texelSize * vec2(-1,  1)).rgb;
    vec3 h = texture(tex, uv + texelSize * vec2( 0,  1)).rgb;
    vec3 i = texture(tex, uv + texelSize * vec2( 1,  1)).rgb;
    
    // Soft min/max
    vec3 minRGB = min(min(min(d, e), min(f, b)), h);
    vec3 maxRGB = max(max(max(d, e), max(f, b)), h);
    
    // Sharpening amount
    vec3 rcpMMinusM = 1.0 / (maxRGB - minRGB + 0.001);
    vec3 ampRGB = saturate(min(minRGB, 1.0 - maxRGB) * rcpMMinusM);
    ampRGB = sqrt(ampRGB);
    
    float peak = -1.0 / mix(8.0, 5.0, sharpness);
    vec3 wRGB = ampRGB * peak;
    vec3 rcpWeightRGB = 1.0 / (1.0 + 4.0 * wRGB);
    
    return saturate((b * wRGB + d * wRGB + f * wRGB + h * wRGB + e) * rcpWeightRGB);
}

#endif // HZ_POSTPROCESS_GLSL
