/**
 * =============================================================================
 * HORIZON ENGINE - SHADER LIBRARY v2.0
 * =============================================================================
 * 
 * File: lighting/lighting.glsl
 * Purpose: Complete lighting calculations
 * =============================================================================
 */

#ifndef HZ_LIGHTING_GLSL
#define HZ_LIGHTING_GLSL

#include "../core/uniforms.glsl"
#include "brdf.glsl"
#include "shadows.glsl"

// =============================================================================
// ATTENUATION
// =============================================================================

// Physical light attenuation (inverse square)
float AttenuationInverseSquare(float distance, float radius) {
    float d = max(distance, 0.01);
    float windowing = sq(saturate(1.0 - sq(d / radius)));
    return windowing / (d * d);
}

// Smooth attenuation (UE4-style)
float AttenuationSmooth(float distance, float radius) {
    float x = saturate(1.0 - pow(distance / radius, 4.0));
    return x * x / (distance * distance + 1.0);
}

// Spot light cone attenuation
float AttenuationSpot(float cosAngle, float innerCutoff, float outerCutoff) {
    return saturate((cosAngle - outerCutoff) / (innerCutoff - outerCutoff));
}

// =============================================================================
// DIRECTIONAL LIGHT
// =============================================================================

vec3 LightDirectional(
    vec3 N, vec3 V,
    vec3 albedo, float metallic, float roughness,
    DirectionalLight light,
    float shadowFactor
) {
    vec3 L = normalize(-light.direction.xyz);
    
    PBRInput input;
    input.albedo = albedo;
    input.metallic = metallic;
    input.roughness = roughness;
    input.N = N;
    input.V = V;
    input.L = L;
    input.lightColor = light.color.xyz;
    input.lightIntensity = light.intensity.x * (1.0 - shadowFactor);
    
    return PBR_DirectLighting(input);
}

// =============================================================================
// POINT LIGHT
// =============================================================================

vec3 LightPoint(
    vec3 worldPos, vec3 N, vec3 V,
    vec3 albedo, float metallic, float roughness,
    PointLight light
) {
    vec3 lightPos = light.positionRadius.xyz;
    float radius = light.positionRadius.w;
    
    vec3 L = lightPos - worldPos;
    float distance = length(L);
    
    if (distance > radius) return vec3(0.0);
    
    L = normalize(L);
    float attenuation = AttenuationSmooth(distance, radius);
    
    PBRInput input;
    input.albedo = albedo;
    input.metallic = metallic;
    input.roughness = roughness;
    input.N = N;
    input.V = V;
    input.L = L;
    input.lightColor = light.colorIntensity.xyz;
    input.lightIntensity = light.colorIntensity.w * attenuation;
    
    return PBR_DirectLighting(input);
}

// =============================================================================
// SPOT LIGHT
// =============================================================================

vec3 LightSpot(
    vec3 worldPos, vec3 N, vec3 V,
    vec3 albedo, float metallic, float roughness,
    SpotLight light
) {
    vec3 lightPos = light.positionRadius.xyz;
    float radius = light.positionRadius.w;
    vec3 spotDir = normalize(light.directionCutoff.xyz);
    float innerCutoff = light.directionCutoff.w;
    float outerCutoff = light.params.x;
    
    vec3 L = lightPos - worldPos;
    float distance = length(L);
    
    if (distance > radius) return vec3(0.0);
    
    L = normalize(L);
    
    // Spot cone
    float cosAngle = dot(L, -spotDir);
    float spotAttenuation = AttenuationSpot(cosAngle, innerCutoff, outerCutoff);
    
    if (spotAttenuation <= 0.0) return vec3(0.0);
    
    float distAttenuation = AttenuationSmooth(distance, radius);
    
    PBRInput input;
    input.albedo = albedo;
    input.metallic = metallic;
    input.roughness = roughness;
    input.N = N;
    input.V = V;
    input.L = L;
    input.lightColor = light.colorIntensity.xyz;
    input.lightIntensity = light.colorIntensity.w * distAttenuation * spotAttenuation;
    
    return PBR_DirectLighting(input);
}

// =============================================================================
// AREA LIGHTS (Approximations)
// =============================================================================

// Sphere area light (Most Significant Bit approximation)
vec3 LightSphere(
    vec3 worldPos, vec3 N, vec3 V, vec3 R,
    vec3 albedo, float metallic, float roughness,
    vec3 lightPos, float lightRadius, vec3 lightColor, float intensity
) {
    vec3 L = lightPos - worldPos;
    float distance = length(L);
    
    // Representative point
    vec3 centerToRay = dot(L, R) * R - L;
    vec3 closestPoint = L + centerToRay * saturate(lightRadius / length(centerToRay));
    
    L = normalize(closestPoint);
    distance = length(closestPoint);
    
    // Energy normalization (roughness-based)
    float alpha = roughness * roughness;
    float alphaPrime = saturate(alpha + lightRadius / (2.0 * distance));
    float energyNorm = alpha / alphaPrime;
    
    float attenuation = 1.0 / (distance * distance + 1.0);
    
    PBRInput input;
    input.albedo = albedo;
    input.metallic = metallic;
    input.roughness = alphaPrime;
    input.N = N;
    input.V = V;
    input.L = L;
    input.lightColor = lightColor;
    input.lightIntensity = intensity * attenuation * energyNorm;
    
    return PBR_DirectLighting(input);
}

// Tube/line area light
vec3 LightTube(
    vec3 worldPos, vec3 N, vec3 V, vec3 R,
    vec3 albedo, float metallic, float roughness,
    vec3 lightStart, vec3 lightEnd, float tubeRadius,
    vec3 lightColor, float intensity
) {
    vec3 L0 = lightStart - worldPos;
    vec3 L1 = lightEnd - worldPos;
    vec3 Ld = L1 - L0;
    
    float RdotLd = dot(R, Ld);
    float L0dotLd = dot(L0, Ld);
    float distLd = length(Ld);
    
    float t = (dot(R, L0) * RdotLd - L0dotLd) / (distLd * distLd - RdotLd * RdotLd);
    t = saturate(t);
    
    vec3 closestPoint = L0 + Ld * t;
    vec3 L = normalize(closestPoint);
    float distance = length(closestPoint);
    
    float attenuation = 1.0 / (distance * distance + 1.0);
    
    PBRInput input;
    input.albedo = albedo;
    input.metallic = metallic;
    input.roughness = roughness;
    input.N = N;
    input.V = V;
    input.L = L;
    input.lightColor = lightColor;
    input.lightIntensity = intensity * attenuation;
    
    return PBR_DirectLighting(input);
}

// =============================================================================
// COMPLETE SURFACE SHADING
// =============================================================================

struct SurfaceData {
    vec3 worldPos;
    vec3 N;
    vec3 V;
    vec3 albedo;
    float metallic;
    float roughness;
    float ao;
    vec3 emission;
};

vec3 CalculateDirectLighting(SurfaceData surface, float shadowFactor) {
    vec3 Lo = vec3(0.0);
    
    // Sun
    Lo += LightDirectional(
        surface.N, surface.V,
        surface.albedo, surface.metallic, surface.roughness,
        hz_Scene.hz_Sun,
        shadowFactor
    );
    
    // Point lights
    for (int i = 0; i < hz_Scene.hz_PointLightCount; i++) {
        Lo += LightPoint(
            surface.worldPos, surface.N, surface.V,
            surface.albedo, surface.metallic, surface.roughness,
            hz_Scene.hz_PointLights[i]
        );
    }
    
    return Lo;
}

#endif // HZ_LIGHTING_GLSL
