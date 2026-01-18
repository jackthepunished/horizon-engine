/**
 * =============================================================================
 * HORIZON ENGINE - SHADER LIBRARY v2.0
 * =============================================================================
 * 
 * File: lighting/brdf.glsl
 * Purpose: Physically-based BRDF functions
 * =============================================================================
 */

#ifndef HZ_BRDF_GLSL
#define HZ_BRDF_GLSL

#include "../core/defines.glsl"
#include "../core/math.glsl"

// =============================================================================
// NORMAL DISTRIBUTION FUNCTIONS (D)
// =============================================================================

// GGX/Trowbridge-Reitz
float D_GGX(float NdotH, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH2 = NdotH * NdotH;
    float denom = NdotH2 * (a2 - 1.0) + 1.0;
    return a2 / (PI * denom * denom);
}

// GGX with anisotropic roughness
float D_GGX_Anisotropic(float NdotH, float TdotH, float BdotH, float roughnessT, float roughnessB) {
    float aT = roughnessT * roughnessT;
    float aB = roughnessB * roughnessB;
    float d = sq(TdotH / aT) + sq(BdotH / aB) + sq(NdotH);
    return 1.0 / (PI * aT * aB * sq(d));
}

// Beckmann distribution
float D_Beckmann(float NdotH, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH2 = NdotH * NdotH;
    return exp((NdotH2 - 1.0) / (a2 * NdotH2)) / (PI * a2 * NdotH2 * NdotH2);
}

// =============================================================================
// GEOMETRY/VISIBILITY FUNCTIONS (G)
// =============================================================================

// Schlick-GGX
float G_SchlickGGX(float NdotV, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;  // For direct lighting
    return NdotV / (NdotV * (1.0 - k) + k);
}

// Smith's method with Schlick-GGX
float G_Smith(float NdotV, float NdotL, float roughness) {
    float ggx1 = G_SchlickGGX(NdotV, roughness);
    float ggx2 = G_SchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

// Smith-GGX height-correlated (more accurate)
float G_SmithCorrelated(float NdotV, float NdotL, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float GGXV = NdotL * sqrt(NdotV * NdotV * (1.0 - a2) + a2);
    float GGXL = NdotV * sqrt(NdotL * NdotL * (1.0 - a2) + a2);
    return 0.5 / (GGXV + GGXL);
}

// Fast approximation of Smith-GGX
float G_SmithFast(float NdotV, float NdotL, float roughness) {
    float a = roughness * roughness;
    float GGXV = NdotL * (NdotV * (1.0 - a) + a);
    float GGXL = NdotV * (NdotL * (1.0 - a) + a);
    return 0.5 / (GGXV + GGXL);
}

// Kelemen visibility (for clearcoat)
float G_Kelemen(float LdotH) {
    return 0.25 / (LdotH * LdotH);
}

// =============================================================================
// FRESNEL FUNCTIONS (F)
// =============================================================================

// Schlick approximation
vec3 F_Schlick(float VdotH, vec3 F0) {
    return F0 + (1.0 - F0) * pow5(1.0 - VdotH);
}

// Schlick with roughness (for IBL)
vec3 F_SchlickRoughness(float VdotH, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow5(1.0 - VdotH);
}

// Exact Fresnel for conductor
vec3 F_ConductorExact(float VdotH, vec3 eta, vec3 k) {
    vec3 k2 = k * k;
    vec3 eta2 = eta * eta;
    float cos2 = VdotH * VdotH;
    float sin2 = 1.0 - cos2;
    
    vec3 t0 = eta2 - k2 - sin2;
    vec3 a2plusb2 = sqrt(t0 * t0 + 4.0 * eta2 * k2);
    vec3 t1 = a2plusb2 + cos2;
    vec3 a = sqrt(0.5 * (a2plusb2 + t0));
    vec3 t2 = 2.0 * a * VdotH;
    vec3 Rs = (t1 - t2) / (t1 + t2);
    
    vec3 t3 = cos2 * a2plusb2 + sin2 * sin2;
    vec3 t4 = t2 * sin2;
    vec3 Rp = Rs * (t3 - t4) / (t3 + t4);
    
    return 0.5 * (Rp + Rs);
}

// =============================================================================
// DIFFUSE BRDF
// =============================================================================

// Lambertian (simple)
vec3 Fd_Lambert(vec3 albedo) {
    return albedo * INV_PI;
}

// Disney diffuse (Burley)
vec3 Fd_Burley(vec3 albedo, float NdotV, float NdotL, float LdotH, float roughness) {
    float fd90 = 0.5 + 2.0 * roughness * LdotH * LdotH;
    float lightScatter = 1.0 + (fd90 - 1.0) * pow5(1.0 - NdotL);
    float viewScatter = 1.0 + (fd90 - 1.0) * pow5(1.0 - NdotV);
    return albedo * INV_PI * lightScatter * viewScatter;
}

// Oren-Nayar (rough diffuse)
vec3 Fd_OrenNayar(vec3 albedo, float NdotV, float NdotL, float VdotL, float roughness) {
    float sigma2 = roughness * roughness;
    float A = 1.0 - 0.5 * sigma2 / (sigma2 + 0.33);
    float B = 0.45 * sigma2 / (sigma2 + 0.09);
    
    float s = VdotL - NdotV * NdotL;
    float t = s > 0.0 ? max(NdotL, NdotV) : 1.0;
    
    return albedo * INV_PI * (A + B * s / t);
}

// =============================================================================
// SPECULAR BRDF (Cook-Torrance)
// =============================================================================

vec3 BRDF_Specular(vec3 F, float D, float G, float NdotV, float NdotL) {
    return (D * G * F) / max(4.0 * NdotV * NdotL, EPSILON);
}

// Single function for complete specular BRDF
vec3 BRDF_CookTorrance(
    float NdotV, float NdotL, float NdotH, float VdotH,
    vec3 F0, float roughness
) {
    float D = D_GGX(NdotH, roughness);
    float G = G_SmithCorrelated(NdotV, NdotL, roughness);
    vec3 F = F_Schlick(VdotH, F0);
    return BRDF_Specular(F, D, G, NdotV, NdotL);
}

// =============================================================================
// SUBSURFACE SCATTERING (Simple approximation)
// =============================================================================

vec3 BRDF_Subsurface(vec3 albedo, float NdotL, float NdotV, float LdotH, float roughness) {
    // Hannahan-Krueger subsurface approximation
    float FL = pow5(1.0 - NdotL);
    float FV = pow5(1.0 - NdotV);
    float Fss90 = LdotH * LdotH * roughness;
    float Fss = mix(1.0, Fss90, FL) * mix(1.0, Fss90, FV);
    float ss = 1.25 * (Fss * (1.0 / (NdotL + NdotV) - 0.5) + 0.5);
    return albedo * INV_PI * ss;
}

// =============================================================================
// CLEAR COAT
// =============================================================================

float BRDF_ClearCoat(float NdotH, float LdotH, float clearcoatRoughness) {
    float D = D_GGX(NdotH, clearcoatRoughness);
    float G = G_Kelemen(LdotH);
    float F = 0.04 + 0.96 * pow5(1.0 - LdotH);  // Polyurethane IOR
    return D * G * F;
}

// =============================================================================
// SHEEN (For cloth)
// =============================================================================

vec3 BRDF_Sheen(vec3 sheenColor, float NdotH, float NdotV, float NdotL) {
    float invRoughness = 1.0 / 0.5;  // Sheen roughness
    float D = (1.0 / (2.0 + invRoughness)) * pow(1.0 - NdotH * NdotH, invRoughness * 0.5);
    return sheenColor * D / (4.0 * (NdotL + NdotV - NdotL * NdotV));
}

// =============================================================================
// IBL UTILITIES
// =============================================================================

// Pre-integrated DFG for IBL (use with BRDF LUT)
vec3 EnvBRDF(vec3 F0, float NdotV, float roughness, sampler2D brdfLUT) {
    vec2 brdf = texture(brdfLUT, vec2(NdotV, roughness)).rg;
    return F0 * brdf.x + brdf.y;
}

// Approximate pre-integrated DFG without LUT
vec3 EnvBRDFApprox(vec3 F0, float NdotV, float roughness) {
    const vec4 c0 = vec4(-1.0, -0.0275, -0.572, 0.022);
    const vec4 c1 = vec4(1.0, 0.0425, 1.04, -0.04);
    vec4 r = roughness * c0 + c1;
    float a004 = min(r.x * r.x, exp2(-9.28 * NdotV)) * r.x + r.y;
    vec2 AB = vec2(-1.04, 1.04) * a004 + r.zw;
    return F0 * AB.x + AB.y;
}

// Importance sampling GGX for IBL prefiltering
vec3 ImportanceSampleGGX(vec2 Xi, vec3 N, float roughness) {
    float a = roughness * roughness;
    
    float phi = TWO_PI * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
    
    vec3 H;
    H.x = cos(phi) * sinTheta;
    H.y = sin(phi) * sinTheta;
    H.z = cosTheta;
    
    vec3 up = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent = normalize(cross(up, N));
    vec3 bitangent = cross(N, tangent);
    
    return tangent * H.x + bitangent * H.y + N * H.z;
}

// =============================================================================
// COMBINED PBR LIGHTING
// =============================================================================

struct PBRInput {
    vec3 albedo;
    float metallic;
    float roughness;
    vec3 N;
    vec3 V;
    vec3 L;
    vec3 lightColor;
    float lightIntensity;
};

vec3 PBR_DirectLighting(PBRInput input) {
    vec3 H = normalize(input.V + input.L);
    
    float NdotV = max(dot(input.N, input.V), EPSILON);
    float NdotL = max(dot(input.N, input.L), 0.0);
    float NdotH = max(dot(input.N, H), 0.0);
    float VdotH = max(dot(input.V, H), 0.0);
    float LdotH = max(dot(input.L, H), 0.0);
    
    // F0 (reflectance at normal incidence)
    vec3 F0 = mix(vec3(DIELECTRIC_F0), input.albedo, input.metallic);
    
    // Specular BRDF
    vec3 F = F_Schlick(VdotH, F0);
    float D = D_GGX(NdotH, input.roughness);
    float G = G_SmithCorrelated(NdotV, NdotL, input.roughness);
    vec3 specular = BRDF_Specular(F, D, G, NdotV, NdotL);
    
    // Diffuse BRDF (energy conserving)
    vec3 kD = (1.0 - F) * (1.0 - input.metallic);
    vec3 diffuse = Fd_Burley(input.albedo, NdotV, NdotL, LdotH, input.roughness);
    
    // Final radiance
    vec3 radiance = input.lightColor * input.lightIntensity;
    return (kD * diffuse + specular) * radiance * NdotL;
}

#endif // HZ_BRDF_GLSL
