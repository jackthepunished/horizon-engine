/**
 * =============================================================================
 * HORIZON ENGINE - SHADER LIBRARY v2.0
 * =============================================================================
 * 
 * File: effects/atmosphere.glsl
 * Purpose: Atmospheric effects (fog, scattering, aerial perspective)
 * =============================================================================
 */

#ifndef HZ_ATMOSPHERE_GLSL
#define HZ_ATMOSPHERE_GLSL

#include "../core/defines.glsl"
#include "../core/uniforms.glsl"

// =============================================================================
// BASIC FOG
// =============================================================================

// Linear fog
float FogLinear(float distance, float start, float end) {
    return saturate((end - distance) / (end - start));
}

// Exponential fog
float FogExponential(float distance, float density) {
    return exp(-density * distance);
}

// Exponential squared fog
float FogExponentialSquared(float distance, float density) {
    float d = density * distance;
    return exp(-d * d);
}

// Height-based fog
float FogHeight(vec3 worldPos, vec3 cameraPos, float density, float heightFalloff) {
    float distance = length(worldPos - cameraPos);
    float heightFog = exp(-heightFalloff * max(worldPos.y, 0.0));
    return 1.0 - exp(-density * distance * heightFog);
}

// =============================================================================
// VOLUMETRIC FOG
// =============================================================================

struct VolumetricFogParams {
    float density;
    float heightFalloff;
    float maxHeight;
    float scatteringCoeff;
    vec3 color;
    vec3 lightDir;
    vec3 lightColor;
};

// Simple volumetric fog with light scattering
vec3 VolumetricFog(
    vec3 rayOrigin,
    vec3 rayDir,
    float rayLength,
    VolumetricFogParams params,
    int steps
) {
    float stepSize = rayLength / float(steps);
    vec3 fogColor = vec3(0.0);
    float transmittance = 1.0;
    
    for (int i = 0; i < steps; i++) {
        float t = (float(i) + 0.5) * stepSize;
        vec3 pos = rayOrigin + rayDir * t;
        
        // Height-based density
        float heightFactor = exp(-params.heightFalloff * max(pos.y, 0.0));
        float localDensity = params.density * heightFactor;
        
        if (pos.y > params.maxHeight) localDensity = 0.0;
        
        // Extinction
        float extinction = localDensity * stepSize;
        float sampleTransmittance = exp(-extinction);
        
        // In-scattering (Henyey-Greenstein phase function)
        float cosAngle = dot(rayDir, -params.lightDir);
        float phase = HenyeyGreenstein(cosAngle, 0.3);
        vec3 inScatter = params.lightColor * params.scatteringCoeff * phase * localDensity;
        
        // Accumulate
        vec3 sampleColor = (inScatter + params.color * localDensity) * stepSize;
        fogColor += transmittance * sampleColor;
        transmittance *= sampleTransmittance;
        
        if (transmittance < 0.01) break;
    }
    
    return fogColor;
}

// =============================================================================
// PHASE FUNCTIONS
// =============================================================================

// Henyey-Greenstein phase function
float HenyeyGreenstein(float cosTheta, float g) {
    float g2 = g * g;
    float denom = 1.0 + g2 - 2.0 * g * cosTheta;
    return (1.0 - g2) / (4.0 * PI * pow(denom, 1.5));
}

// Rayleigh phase function
float Rayleigh(float cosTheta) {
    return (3.0 / (16.0 * PI)) * (1.0 + cosTheta * cosTheta);
}

// Cornette-Shanks (improved HG)
float CornetteShanks(float cosTheta, float g) {
    float g2 = g * g;
    float num = 3.0 * (1.0 - g2) * (1.0 + cosTheta * cosTheta);
    float denom = 2.0 * (2.0 + g2) * pow(1.0 + g2 - 2.0 * g * cosTheta, 1.5);
    return num / denom;
}

// Mie phase function approximation
float MiePhase(float cosTheta, float g) {
    return HenyeyGreenstein(cosTheta, g);
}

// =============================================================================
// ATMOSPHERIC SCATTERING (Physically-based)
// =============================================================================

// Rayleigh scattering coefficients for Earth atmosphere
const vec3 RAYLEIGH_COEFF = vec3(5.8e-6, 13.5e-6, 33.1e-6);
const float MIE_COEFF = 21e-6;
const float RAYLEIGH_SCALE_HEIGHT = 8400.0;
const float MIE_SCALE_HEIGHT = 1200.0;
const float EARTH_RADIUS = 6371000.0;
const float ATMOSPHERE_RADIUS = 6471000.0;

// Optical depth along ray segment
vec2 OpticalDepth(vec3 rayOrigin, vec3 rayDir, float rayLength, int samples) {
    float stepSize = rayLength / float(samples);
    vec2 opticalDepth = vec2(0.0);
    
    for (int i = 0; i < samples; i++) {
        vec3 samplePos = rayOrigin + rayDir * (float(i) + 0.5) * stepSize;
        float altitude = length(samplePos) - EARTH_RADIUS;
        
        opticalDepth.x += exp(-altitude / RAYLEIGH_SCALE_HEIGHT);
        opticalDepth.y += exp(-altitude / MIE_SCALE_HEIGHT);
    }
    
    return opticalDepth * stepSize;
}

// Simple sky color based on view direction and sun
vec3 AtmosphericScattering(vec3 viewDir, vec3 sunDir) {
    float cosAngle = dot(viewDir, sunDir);
    
    // Rayleigh scattering (blue sky)
    vec3 rayleigh = RAYLEIGH_COEFF * Rayleigh(cosAngle);
    
    // Mie scattering (sun glow)
    float mie = MiePhase(cosAngle, 0.76) * MIE_COEFF;
    
    // Simple optical depth approximation
    float zenithAngle = max(viewDir.y, 0.01);
    float opticalDepth = 1.0 / zenithAngle;
    
    vec3 extinction = exp(-opticalDepth * (RAYLEIGH_COEFF + MIE_COEFF));
    vec3 scatter = (rayleigh + mie) * (1.0 - extinction);
    
    return scatter * 50.0;  // Intensity multiplier
}

// =============================================================================
// AERIAL PERSPECTIVE
// =============================================================================

vec3 ApplyAerialPerspective(vec3 color, float distance, vec3 viewDir, vec3 atmosColor) {
    float density = 0.00001;
    float factor = 1.0 - exp(-distance * density);
    return mix(color, atmosColor, factor);
}

// =============================================================================
// APPLY FOG
// =============================================================================

vec3 ApplyFog(vec3 color, vec3 worldPos) {
    if (hz_Scene.hz_FogParams.w < 0.5) return color;
    
    vec3 cameraPos = hz_Camera.hz_CameraPosition.xyz;
    float distance = length(worldPos - cameraPos);
    
    float fogDensity = hz_Scene.hz_FogParams.x;
    float fogGradient = hz_Scene.hz_FogParams.y;
    vec3 fogColor = hz_Scene.hz_FogColor.xyz;
    
    float fogFactor = 1.0 - exp(-pow(distance * fogDensity, fogGradient));
    fogFactor = saturate(fogFactor);
    
    return mix(color, fogColor, fogFactor);
}

#endif // HZ_ATMOSPHERE_GLSL
