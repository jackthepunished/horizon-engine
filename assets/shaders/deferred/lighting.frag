#version 410 core

/**
 * Deferred Lighting Pass - Fragment Shader
 * Performs PBR lighting using G-Buffer data
 * Supports: Directional (sun), Point, Spot lights + CSM shadows
 */

out vec4 FragColor;

in vec2 TexCoord;

// G-Buffer textures
uniform sampler2D gAlbedoMetallic;
uniform sampler2D gNormalRoughness;
uniform sampler2D gEmissionID;
uniform sampler2D gDepth;

// Shadow map (texture array for CSM)
uniform sampler2DArrayShadow shadowMap;

// Camera
uniform mat4 u_InverseView;
uniform mat4 u_InverseProjection;
uniform vec3 u_ViewPos;

// CSM data
#define MAX_CASCADES 4
uniform int u_CascadeCount;
uniform float u_CascadeSplits[MAX_CASCADES];
uniform mat4 u_LightSpaceMatrices[MAX_CASCADES];
uniform float u_ShadowBias;
uniform float u_CascadeBlendDistance;

// Sun light
uniform vec3 u_SunDirection;
uniform vec3 u_SunColor;
uniform float u_SunIntensity;

// Point lights
#define MAX_POINT_LIGHTS 128
uniform int u_PointLightCount;
uniform vec4 u_PointLightPositions[MAX_POINT_LIGHTS];  // xyz = pos, w = radius
uniform vec4 u_PointLightColors[MAX_POINT_LIGHTS];     // xyz = color, w = intensity

// Spot lights
#define MAX_SPOT_LIGHTS 32
uniform int u_SpotLightCount;
uniform vec4 u_SpotLightPositions[MAX_SPOT_LIGHTS];
uniform vec4 u_SpotLightDirections[MAX_SPOT_LIGHTS];  // xyz = dir, w = cutoff
uniform vec4 u_SpotLightColors[MAX_SPOT_LIGHTS];
uniform vec4 u_SpotLightParams[MAX_SPOT_LIGHTS];      // x = outer cutoff, y = intensity

// IBL
uniform samplerCube u_IrradianceMap;
uniform samplerCube u_PrefilteredMap;
uniform sampler2D u_BRDFLUT;
uniform float u_IBLIntensity;

// SSAO
uniform sampler2D u_SSAOTexture;
uniform bool u_UseSSAO;

const float PI = 3.14159265359;

// Decode octahedron normal
vec3 decodeOctahedron(vec2 f) {
    f = f * 2.0 - 1.0;
    vec3 n = vec3(f.x, f.y, 1.0 - abs(f.x) - abs(f.y));
    float t = clamp(-n.z, 0.0, 1.0);
    n.x += n.x >= 0.0 ? -t : t;
    n.y += n.y >= 0.0 ? -t : t;
    return normalize(n);
}

// Reconstruct world position from depth
vec3 reconstructWorldPos(vec2 uv, float depth) {
    vec4 clipPos = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 viewPos = u_InverseProjection * clipPos;
    viewPos /= viewPos.w;
    vec4 worldPos = u_InverseView * viewPos;
    return worldPos.xyz;
}

// PBR functions
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    return a2 / (PI * denom * denom);
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    return GeometrySchlickGGX(NdotV, roughness) * GeometrySchlickGGX(NdotL, roughness);
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// CSM shadow calculation
float calculateShadow(vec3 worldPos, vec3 N) {
    // Find which cascade to use
    vec4 viewPos = u_InverseView * vec4(worldPos - u_ViewPos, 0.0);
    float depth = -viewPos.z;
    
    int cascade = u_CascadeCount - 1;
    for (int i = 0; i < u_CascadeCount; ++i) {
        if (depth < u_CascadeSplits[i]) {
            cascade = i;
            break;
        }
    }
    
    // Project to light space
    vec4 lightSpacePos = u_LightSpaceMatrices[cascade] * vec4(worldPos, 1.0);
    vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
    projCoords = projCoords * 0.5 + 0.5;
    
    if (projCoords.z > 1.0) return 0.0;
    
    // Bias based on surface angle
    float bias = max(u_ShadowBias * (1.0 - dot(N, -u_SunDirection)), u_ShadowBias * 0.1);
    float currentDepth = projCoords.z - bias;
    
    // PCF sampling
    float shadow = 0.0;
    vec2 texelSize = 1.0 / vec2(textureSize(shadowMap, 0).xy);
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            vec2 offset = vec2(x, y) * texelSize;
            // Use shadow sampler for hardware comparison
            shadow += texture(shadowMap, vec4(projCoords.xy + offset, float(cascade), currentDepth));
        }
    }
    shadow /= 9.0;
    
    // Cascade blending
    if (cascade < u_CascadeCount - 1) {
        float fade = clamp((u_CascadeSplits[cascade] - depth) / u_CascadeBlendDistance, 0.0, 1.0);
        if (fade < 1.0) {
            // Sample next cascade
            vec4 nextLightSpacePos = u_LightSpaceMatrices[cascade + 1] * vec4(worldPos, 1.0);
            vec3 nextProjCoords = nextLightSpacePos.xyz / nextLightSpacePos.w;
            nextProjCoords = nextProjCoords * 0.5 + 0.5;
            float nextCurrentDepth = nextProjCoords.z - bias;
            
            float nextShadow = 0.0;
            for (int x = -1; x <= 1; ++x) {
                for (int y = -1; y <= 1; ++y) {
                    vec2 offset = vec2(x, y) * texelSize;
                    nextShadow += texture(shadowMap, vec4(nextProjCoords.xy + offset, float(cascade + 1), nextCurrentDepth));
                }
            }
            nextShadow /= 9.0;
            
            shadow = mix(nextShadow, shadow, fade);
        }
    }
    
    return 1.0 - shadow;
}

// Calculate lighting contribution
vec3 calculateLighting(vec3 worldPos, vec3 N, vec3 V, vec3 albedo, float metallic, float roughness, float ao) {
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 Lo = vec3(0.0);
    
    // Directional light (sun) with shadows
    {
        vec3 L = normalize(-u_SunDirection);
        vec3 H = normalize(V + L);
        float NdotL = max(dot(N, L), 0.0);
        
        float shadow = calculateShadow(worldPos, N);
        
        float D = DistributionGGX(N, H, roughness);
        float G = GeometrySmith(N, V, L, roughness);
        vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
        
        vec3 numerator = D * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * NdotL + 0.0001;
        vec3 specular = numerator / denominator;
        
        vec3 kS = F;
        vec3 kD = (1.0 - kS) * (1.0 - metallic);
        
        Lo += (kD * albedo / PI + specular) * u_SunColor * u_SunIntensity * NdotL * (1.0 - shadow);
    }
    
    // Point lights
    for (int i = 0; i < u_PointLightCount && i < MAX_POINT_LIGHTS; ++i) {
        vec3 lightPos = u_PointLightPositions[i].xyz;
        float radius = u_PointLightPositions[i].w;
        vec3 lightColor = u_PointLightColors[i].xyz;
        float intensity = u_PointLightColors[i].w;
        
        vec3 L = lightPos - worldPos;
        float distance = length(L);
        if (distance > radius) continue;
        
        L = normalize(L);
        vec3 H = normalize(V + L);
        float NdotL = max(dot(N, L), 0.0);
        
        // Attenuation
        float attenuation = pow(clamp(1.0 - pow(distance / radius, 4.0), 0.0, 1.0), 2.0) / (distance * distance + 1.0);
        
        float D = DistributionGGX(N, H, roughness);
        float G = GeometrySmith(N, V, L, roughness);
        vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
        
        vec3 numerator = D * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * NdotL + 0.0001;
        vec3 specular = numerator / denominator;
        
        vec3 kS = F;
        vec3 kD = (1.0 - kS) * (1.0 - metallic);
        
        Lo += (kD * albedo / PI + specular) * lightColor * intensity * NdotL * attenuation;
    }
    
    // Spot lights
    for (int i = 0; i < u_SpotLightCount && i < MAX_SPOT_LIGHTS; ++i) {
        vec3 lightPos = u_SpotLightPositions[i].xyz;
        float radius = u_SpotLightPositions[i].w;
        vec3 spotDir = u_SpotLightDirections[i].xyz;
        float cutoff = u_SpotLightDirections[i].w;
        vec3 lightColor = u_SpotLightColors[i].xyz;
        float outerCutoff = u_SpotLightParams[i].x;
        float intensity = u_SpotLightParams[i].y;
        
        vec3 L = lightPos - worldPos;
        float distance = length(L);
        if (distance > radius) continue;
        
        L = normalize(L);
        
        float theta = dot(L, normalize(-spotDir));
        float epsilon = cutoff - outerCutoff;
        float spotIntensity = clamp((theta - outerCutoff) / epsilon, 0.0, 1.0);
        
        if (spotIntensity <= 0.0) continue;
        
        vec3 H = normalize(V + L);
        float NdotL = max(dot(N, L), 0.0);
        
        float attenuation = pow(clamp(1.0 - pow(distance / radius, 4.0), 0.0, 1.0), 2.0) / (distance * distance + 1.0);
        
        float D = DistributionGGX(N, H, roughness);
        float G = GeometrySmith(N, V, L, roughness);
        vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
        
        vec3 numerator = D * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * NdotL + 0.0001;
        vec3 specular = numerator / denominator;
        
        vec3 kS = F;
        vec3 kD = (1.0 - kS) * (1.0 - metallic);
        
        Lo += (kD * albedo / PI + specular) * lightColor * intensity * NdotL * attenuation * spotIntensity;
    }
    
    // IBL ambient
    vec3 F = fresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);
    vec3 kS = F;
    vec3 kD = (1.0 - kS) * (1.0 - metallic);
    
    vec3 irradiance = texture(u_IrradianceMap, N).rgb;
    vec3 diffuse = irradiance * albedo;
    
    const float MAX_REFLECTION_LOD = 4.0;
    vec3 R = reflect(-V, N);
    vec3 prefilteredColor = textureLod(u_PrefilteredMap, R, roughness * MAX_REFLECTION_LOD).rgb;
    vec2 brdf = texture(u_BRDFLUT, vec2(max(dot(N, V), 0.0), roughness)).rg;
    vec3 specular = prefilteredColor * (F * brdf.x + brdf.y);
    
    vec3 ambient = (kD * diffuse + specular) * ao * u_IBLIntensity;
    
    // Apply SSAO
    if (u_UseSSAO) {
        float ssao = texture(u_SSAOTexture, TexCoord).r;
        ambient *= ssao;
    }
    
    return ambient + Lo;
}

void main() {
    // Sample G-Buffer
    vec4 albedoMetallic = texture(gAlbedoMetallic, TexCoord);
    vec4 normalRoughness = texture(gNormalRoughness, TexCoord);
    vec4 emissionID = texture(gEmissionID, TexCoord);
    float depth = texture(gDepth, TexCoord).r;
    
    // Early out for sky
    if (depth >= 1.0) {
        FragColor = vec4(0.0);
        return;
    }
    
    // Unpack G-Buffer
    vec3 albedo = albedoMetallic.rgb;
    float metallic = albedoMetallic.a;
    vec3 N = decodeOctahedron(normalRoughness.rg);
    float roughness = normalRoughness.b;
    float ao = normalRoughness.a;
    vec3 emission = emissionID.rgb;
    
    // Reconstruct world position
    vec3 worldPos = reconstructWorldPos(TexCoord, depth);
    vec3 V = normalize(u_ViewPos - worldPos);
    
    // Calculate lighting
    vec3 color = calculateLighting(worldPos, N, V, albedo, metallic, roughness, ao);
    
    // Add emission
    color += emission;
    
    FragColor = vec4(color, 1.0);
}
