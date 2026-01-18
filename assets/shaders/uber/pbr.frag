#version 410 core

/**
 * =============================================================================
 * HORIZON ENGINE - Uber PBR Fragment Shader
 * =============================================================================
 * 
 * Full PBR pipeline with:
 * - Cook-Torrance BRDF
 * - Image-Based Lighting
 * - Cascaded Shadow Maps
 * - SSAO
 * - Emission
 * - Multiple light types
 * =============================================================================
 */

// =============================================================================
// INLINE LIBRARY (OpenGL 4.1 doesn't support #include)
// =============================================================================

#define PI 3.14159265359
#define INV_PI 0.31830988618
#define EPSILON 0.0001
#define saturate(x) clamp(x, 0.0, 1.0)
#define sq(x) ((x) * (x))
#define pow5(x) ((x) * (x) * (x) * (x) * (x))

const float MAX_REFLECTION_LOD = 4.0;
const float DIELECTRIC_F0 = 0.04;

// =============================================================================
// INPUTS
// =============================================================================

in VS_OUT {
    vec3 WorldPos;
    vec3 Normal;
    vec2 TexCoord;
    vec3 Tangent;
    vec3 Bitangent;
    vec4 LightSpacePos;
    vec4 ClipPos;
    vec4 PrevClipPos;
    float ViewDepth;
} fs_in;

// =============================================================================
// OUTPUTS
// =============================================================================

layout(location = 0) out vec4 FragColor;
layout(location = 1) out vec2 VelocityOut;  // Motion vectors

// =============================================================================
// UNIFORMS - Camera
// =============================================================================

layout(std140) uniform CameraData {
    mat4 u_View;
    mat4 u_Projection;
    mat4 u_ViewProjection;
    mat4 u_InverseView;
    mat4 u_InverseProjection;
    mat4 u_InverseViewProjection;
    mat4 u_PrevViewProjection;
    vec4 u_CameraPosition;
    vec4 u_CameraDirection;
    vec4 u_ViewportSize;
    vec4 u_ClipPlanes;
    vec4 u_JitterOffset;
};

// =============================================================================
// UNIFORMS - Lights
// =============================================================================

struct DirectionalLight {
    vec4 direction;
    vec4 color;
    vec4 intensity;
};

struct PointLight {
    vec4 positionRadius;
    vec4 colorIntensity;
};

#define MAX_POINT_LIGHTS 16

layout(std140) uniform SceneData {
    DirectionalLight u_Sun;
    vec4 u_AmbientColor;
    vec4 u_TimeParams;
    vec4 u_FogParams;
    vec4 u_FogColor;
    int u_PointLightCount;
    int _pad1;
    int _pad2;
    int _pad3;
    PointLight u_PointLights[MAX_POINT_LIGHTS];
};

// =============================================================================
// UNIFORMS - Material
// =============================================================================

uniform sampler2D u_AlbedoMap;
uniform sampler2D u_NormalMap;
uniform sampler2D u_MetallicRoughnessMap;
uniform sampler2D u_AOMap;
uniform sampler2D u_EmissiveMap;

uniform vec4 u_BaseColor;
uniform float u_Metallic;
uniform float u_Roughness;
uniform float u_AO;
uniform vec3 u_EmissiveColor;
uniform float u_EmissiveStrength;
uniform float u_UVScale;

uniform int u_MaterialFlags;
#define MAT_HAS_ALBEDO    (1 << 0)
#define MAT_HAS_NORMAL    (1 << 1)
#define MAT_HAS_METALLIC  (1 << 2)
#define MAT_HAS_ROUGHNESS (1 << 3)
#define MAT_HAS_AO        (1 << 4)
#define MAT_HAS_EMISSIVE  (1 << 5)

// =============================================================================
// UNIFORMS - Environment
// =============================================================================

uniform samplerCube u_IrradianceMap;
uniform samplerCube u_PrefilterMap;
uniform sampler2D u_BRDF_LUT;
uniform bool u_UseIBL;
uniform float u_IBLIntensity;

// =============================================================================
// UNIFORMS - Shadows
// =============================================================================

uniform sampler2D u_ShadowMap;
uniform float u_ShadowBias;
uniform float u_ShadowStrength;

// =============================================================================
// UNIFORMS - SSAO
// =============================================================================

uniform sampler2D u_SSAOMap;
uniform bool u_UseSSAO;

// =============================================================================
// PBR FUNCTIONS
// =============================================================================

// GGX Normal Distribution
float D_GGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float denom = NdotH2 * (a2 - 1.0) + 1.0;
    return a2 / (PI * denom * denom);
}

// Smith-GGX Geometry
float G_SchlickGGX(float NdotV, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float G_Smith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    return G_SchlickGGX(NdotV, roughness) * G_SchlickGGX(NdotL, roughness);
}

// Fresnel-Schlick
vec3 F_Schlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow5(1.0 - cosTheta);
}

vec3 F_SchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow5(1.0 - cosTheta);
}

// =============================================================================
// SHADOW FUNCTIONS
// =============================================================================

float CalculateShadow(vec4 lightSpacePos, vec3 N, vec3 L) {
    vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
    projCoords = projCoords * 0.5 + 0.5;
    
    if (projCoords.z > 1.0) return 0.0;
    
    float bias = max(u_ShadowBias * (1.0 - dot(N, L)), u_ShadowBias * 0.1);
    
    // PCF 3x3
    float shadow = 0.0;
    vec2 texelSize = 1.0 / vec2(textureSize(u_ShadowMap, 0));
    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            float depth = texture(u_ShadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += projCoords.z - bias > depth ? 1.0 : 0.0;
        }
    }
    
    return (shadow / 9.0) * u_ShadowStrength;
}

// =============================================================================
// LIGHTING FUNCTIONS
// =============================================================================

vec3 CalculateDirectLight(vec3 L, vec3 lightColor, float intensity,
                           vec3 N, vec3 V, vec3 albedo, float metallic, float roughness, vec3 F0) {
    vec3 H = normalize(V + L);
    
    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), EPSILON);
    float NdotH = max(dot(N, H), 0.0);
    float VdotH = max(dot(V, H), 0.0);
    
    // Cook-Torrance BRDF
    float D = D_GGX(N, H, roughness);
    float G = G_Smith(N, V, L, roughness);
    vec3 F = F_Schlick(VdotH, F0);
    
    vec3 specular = (D * G * F) / max(4.0 * NdotV * NdotL, EPSILON);
    
    vec3 kD = (1.0 - F) * (1.0 - metallic);
    vec3 diffuse = kD * albedo * INV_PI;
    
    return (diffuse + specular) * lightColor * intensity * NdotL;
}

vec3 CalculateIBL(vec3 N, vec3 V, vec3 R, vec3 albedo, float metallic, float roughness, float ao, vec3 F0) {
    if (!u_UseIBL) {
        return u_AmbientColor.xyz * albedo * ao;
    }
    
    float NdotV = max(dot(N, V), EPSILON);
    vec3 F = F_SchlickRoughness(NdotV, F0, roughness);
    
    vec3 kD = (1.0 - F) * (1.0 - metallic);
    
    // Diffuse IBL
    vec3 irradiance = texture(u_IrradianceMap, N).rgb;
    vec3 diffuse = irradiance * albedo;
    
    // Specular IBL
    vec3 prefilteredColor = textureLod(u_PrefilterMap, R, roughness * MAX_REFLECTION_LOD).rgb;
    vec2 brdf = texture(u_BRDF_LUT, vec2(NdotV, roughness)).rg;
    vec3 specular = prefilteredColor * (F * brdf.x + brdf.y);
    
    return (kD * diffuse + specular) * ao * u_IBLIntensity;
}

// =============================================================================
// NORMAL MAPPING
// =============================================================================

vec3 GetNormal() {
    vec3 N = normalize(fs_in.Normal);
    
    if ((u_MaterialFlags & MAT_HAS_NORMAL) != 0) {
        vec3 tangentNormal = texture(u_NormalMap, fs_in.TexCoord * u_UVScale).rgb * 2.0 - 1.0;
        
        vec3 T = normalize(fs_in.Tangent);
        vec3 B = normalize(fs_in.Bitangent);
        mat3 TBN = mat3(T, B, N);
        
        N = normalize(TBN * tangentNormal);
    }
    
    return N;
}

// =============================================================================
// FOG
// =============================================================================

vec3 ApplyFog(vec3 color) {
    if (u_FogParams.w < 0.5) return color;
    
    float distance = length(fs_in.WorldPos - u_CameraPosition.xyz);
    float fogFactor = 1.0 - exp(-pow(distance * u_FogParams.x, u_FogParams.y));
    return mix(color, u_FogColor.xyz, saturate(fogFactor));
}

// =============================================================================
// MAIN
// =============================================================================

void main() {
    vec2 uv = fs_in.TexCoord * u_UVScale;
    
    // Sample material
    vec3 albedo = u_BaseColor.rgb;
    if ((u_MaterialFlags & MAT_HAS_ALBEDO) != 0) {
        vec4 albedoSample = texture(u_AlbedoMap, uv);
        albedo = pow(albedoSample.rgb, vec3(2.2));  // sRGB to linear
    }
    
    float metallic = u_Metallic;
    float roughness = u_Roughness;
    if ((u_MaterialFlags & MAT_HAS_METALLIC) != 0) {
        vec3 mr = texture(u_MetallicRoughnessMap, uv).rgb;
        metallic = mr.b;
        roughness = mr.g;
    }
    roughness = max(roughness, 0.04);
    
    float ao = u_AO;
    if ((u_MaterialFlags & MAT_HAS_AO) != 0) {
        ao = texture(u_AOMap, uv).r;
    }
    
    vec3 emission = u_EmissiveColor * u_EmissiveStrength;
    if ((u_MaterialFlags & MAT_HAS_EMISSIVE) != 0) {
        emission = texture(u_EmissiveMap, uv).rgb * u_EmissiveStrength;
    }
    
    // Vectors
    vec3 N = GetNormal();
    vec3 V = normalize(u_CameraPosition.xyz - fs_in.WorldPos);
    vec3 R = reflect(-V, N);
    
    // F0 (reflectance at normal incidence)
    vec3 F0 = mix(vec3(DIELECTRIC_F0), albedo, metallic);
    
    // Lighting accumulator
    vec3 Lo = vec3(0.0);
    
    // Directional light (sun) with shadows
    vec3 L_sun = normalize(-u_Sun.direction.xyz);
    float shadow = CalculateShadow(fs_in.LightSpacePos, N, L_sun);
    Lo += CalculateDirectLight(L_sun, u_Sun.color.xyz, u_Sun.intensity.x * (1.0 - shadow),
                                N, V, albedo, metallic, roughness, F0);
    
    // Point lights
    for (int i = 0; i < u_PointLightCount && i < MAX_POINT_LIGHTS; i++) {
        vec3 lightPos = u_PointLights[i].positionRadius.xyz;
        float radius = u_PointLights[i].positionRadius.w;
        vec3 lightColor = u_PointLights[i].colorIntensity.xyz;
        float intensity = u_PointLights[i].colorIntensity.w;
        
        vec3 L = lightPos - fs_in.WorldPos;
        float distance = length(L);
        
        if (distance < radius) {
            L = normalize(L);
            float attenuation = sq(saturate(1.0 - sq(sq(distance / radius)))) / (distance * distance + 1.0);
            Lo += CalculateDirectLight(L, lightColor, intensity * attenuation,
                                        N, V, albedo, metallic, roughness, F0);
        }
    }
    
    // IBL ambient
    vec3 ambient = CalculateIBL(N, V, R, albedo, metallic, roughness, ao, F0);
    
    // SSAO
    if (u_UseSSAO) {
        vec2 screenUV = gl_FragCoord.xy * u_ViewportSize.zw;
        float ssao = texture(u_SSAOMap, screenUV).r;
        ambient *= ssao;
    }
    
    // Final color
    vec3 color = ambient + Lo + emission;
    
    // Fog
    color = ApplyFog(color);
    
    FragColor = vec4(color, 1.0);
    
    // Motion vectors (for TAA)
    vec2 currentPos = fs_in.ClipPos.xy / fs_in.ClipPos.w;
    vec2 prevPos = fs_in.PrevClipPos.xy / fs_in.PrevClipPos.w;
    VelocityOut = (currentPos - prevPos) * 0.5;
}
