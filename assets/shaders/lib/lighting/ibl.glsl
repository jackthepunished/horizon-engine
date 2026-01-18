/**
 * =============================================================================
 * HORIZON ENGINE - SHADER LIBRARY v2.0
 * =============================================================================
 * 
 * File: lighting/ibl.glsl
 * Purpose: Image-Based Lighting utilities
 * =============================================================================
 */

#ifndef HZ_IBL_GLSL
#define HZ_IBL_GLSL

#include "../core/defines.glsl"
#include "brdf.glsl"

// =============================================================================
// IBL SAMPLERS
// =============================================================================

uniform samplerCube hz_IrradianceMap;
uniform samplerCube hz_PrefilteredMap;
uniform sampler2D hz_BRDF_LUT;
uniform float hz_IBLIntensity;
uniform bool hz_UseIBL;

// =============================================================================
// IBL SAMPLING
// =============================================================================

// Sample irradiance (diffuse IBL)
vec3 SampleIrradiance(vec3 N) {
    return texture(hz_IrradianceMap, N).rgb;
}

// Sample prefiltered environment (specular IBL)
vec3 SamplePrefiltered(vec3 R, float roughness) {
    float lod = roughness * MAX_REFLECTION_LOD;
    return textureLod(hz_PrefilteredMap, R, lod).rgb;
}

// Sample BRDF LUT
vec2 SampleBRDF_LUT(float NdotV, float roughness) {
    return texture(hz_BRDF_LUT, vec2(NdotV, roughness)).rg;
}

// =============================================================================
// IBL CALCULATIONS
// =============================================================================

vec3 CalculateIBL_Diffuse(vec3 N, vec3 albedo, float metallic, float ao) {
    vec3 F0 = mix(vec3(DIELECTRIC_F0), albedo, metallic);
    vec3 kD = (1.0 - F0) * (1.0 - metallic);
    
    vec3 irradiance = SampleIrradiance(N);
    return kD * irradiance * albedo * ao;
}

vec3 CalculateIBL_Specular(vec3 N, vec3 V, vec3 albedo, float metallic, float roughness, float ao) {
    float NdotV = max(dot(N, V), EPSILON);
    vec3 R = reflect(-V, N);
    
    vec3 F0 = mix(vec3(DIELECTRIC_F0), albedo, metallic);
    vec3 F = F_SchlickRoughness(NdotV, F0, roughness);
    
    vec3 prefilteredColor = SamplePrefiltered(R, roughness);
    vec2 brdf = SampleBRDF_LUT(NdotV, roughness);
    
    return prefilteredColor * (F * brdf.x + brdf.y) * ao;
}

vec3 CalculateIBL(vec3 N, vec3 V, vec3 albedo, float metallic, float roughness, float ao) {
    if (!hz_UseIBL) {
        return hz_Scene.hz_AmbientColor.xyz * albedo * ao * hz_Scene.hz_AmbientColor.w;
    }
    
    vec3 diffuse = CalculateIBL_Diffuse(N, albedo, metallic, ao);
    vec3 specular = CalculateIBL_Specular(N, V, albedo, metallic, roughness, ao);
    
    return (diffuse + specular) * hz_IBLIntensity;
}

// =============================================================================
// PARALLAX CORRECTED CUBEMAPS
// =============================================================================

// For local reflection probes
vec3 ParallaxCorrectCubemap(vec3 R, vec3 worldPos, vec3 boxMin, vec3 boxMax, vec3 probePos) {
    // Find intersection with box
    vec3 firstPlane = (boxMax - worldPos) / R;
    vec3 secondPlane = (boxMin - worldPos) / R;
    vec3 furthestPlane = max(firstPlane, secondPlane);
    float distance = min(min(furthestPlane.x, furthestPlane.y), furthestPlane.z);
    
    // Corrected reflection
    vec3 intersectPos = worldPos + R * distance;
    return intersectPos - probePos;
}

// =============================================================================
// SPHERICAL HARMONICS (L2)
// =============================================================================

struct SH9 {
    vec3 coefficients[9];
};

vec3 EvaluateSH(SH9 sh, vec3 N) {
    // Band 0
    vec3 result = sh.coefficients[0] * 0.282095;
    
    // Band 1
    result += sh.coefficients[1] * 0.488603 * N.y;
    result += sh.coefficients[2] * 0.488603 * N.z;
    result += sh.coefficients[3] * 0.488603 * N.x;
    
    // Band 2
    result += sh.coefficients[4] * 1.092548 * N.x * N.y;
    result += sh.coefficients[5] * 1.092548 * N.y * N.z;
    result += sh.coefficients[6] * 0.315392 * (3.0 * N.z * N.z - 1.0);
    result += sh.coefficients[7] * 1.092548 * N.x * N.z;
    result += sh.coefficients[8] * 0.546274 * (N.x * N.x - N.y * N.y);
    
    return max(result, vec3(0.0));
}

// =============================================================================
// SCREEN SPACE REFLECTIONS HELPER
// =============================================================================

// Jittered reflection direction for roughness
vec3 GetJitteredReflection(vec3 R, vec3 N, float roughness, vec2 noise) {
    if (roughness < 0.01) return R;
    
    vec3 T, B;
    createBasis(N, T, B);
    
    float a = roughness * roughness;
    float phi = TWO_PI * noise.x;
    float cosTheta = sqrt((1.0 - noise.y) / (1.0 + (a * a - 1.0) * noise.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
    
    vec3 H = T * (cos(phi) * sinTheta) + B * (sin(phi) * sinTheta) + N * cosTheta;
    return reflect(-normalize(R + N), H);
}

#endif // HZ_IBL_GLSL
