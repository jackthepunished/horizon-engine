#version 410 core

/**
 * Deferred Geometry Pass - Fragment Shader
 * Outputs to G-Buffer MRT:
 *   RT0: Albedo.rgb, Metallic
 *   RT1: Normal.rg (octahedron), Roughness, AO
 *   RT2: Emission.rgb, MaterialID
 */

layout(location = 0) out vec4 gAlbedoMetallic;
layout(location = 1) out vec4 gNormalRoughness;
layout(location = 2) out vec4 gEmissionID;

in VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoord;
    vec3 Tangent;
    vec4 ClipPos;
    vec4 PrevClipPos;
} fs_in;

// Material textures
uniform sampler2D u_AlbedoMap;
uniform sampler2D u_NormalMap;
uniform sampler2D u_MetallicRoughnessMap;
uniform sampler2D u_AOMap;
uniform sampler2D u_EmissionMap;

// Material properties
uniform vec3 u_AlbedoColor;
uniform float u_Metallic;
uniform float u_Roughness;
uniform vec3 u_EmissionColor;
uniform float u_EmissionStrength;
uniform float u_MaterialID;

// Flags
uniform bool u_UseAlbedoMap;
uniform bool u_UseNormalMap;
uniform bool u_UseMetallicRoughnessMap;
uniform bool u_UseAOMap;
uniform bool u_UseEmissionMap;

// Octahedron normal encoding
vec2 encodeOctahedron(vec3 n) {
    n /= (abs(n.x) + abs(n.y) + abs(n.z));
    if (n.z < 0.0) {
        n.xy = (1.0 - abs(n.yx)) * vec2(n.x >= 0.0 ? 1.0 : -1.0, n.y >= 0.0 ? 1.0 : -1.0);
    }
    return n.xy * 0.5 + 0.5;
}

vec3 getNormalFromMap() {
    vec3 tangentNormal = texture(u_NormalMap, fs_in.TexCoord).rgb * 2.0 - 1.0;
    
    vec3 N = normalize(fs_in.Normal);
    vec3 T = normalize(fs_in.Tangent);
    T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T);
    
    mat3 TBN = mat3(T, B, N);
    return normalize(TBN * tangentNormal);
}

void main() {
    // Albedo
    vec3 albedo = u_AlbedoColor;
    if (u_UseAlbedoMap) {
        albedo = texture(u_AlbedoMap, fs_in.TexCoord).rgb;
    }
    
    // Normal
    vec3 normal = normalize(fs_in.Normal);
    if (u_UseNormalMap) {
        normal = getNormalFromMap();
    }
    
    // Metallic / Roughness (often packed in one texture: R=AO, G=Roughness, B=Metallic)
    float metallic = u_Metallic;
    float roughness = u_Roughness;
    if (u_UseMetallicRoughnessMap) {
        vec3 mr = texture(u_MetallicRoughnessMap, fs_in.TexCoord).rgb;
        metallic = mr.b;
        roughness = mr.g;
    }
    roughness = clamp(roughness, 0.04, 1.0);
    
    // AO
    float ao = 1.0;
    if (u_UseAOMap) {
        ao = texture(u_AOMap, fs_in.TexCoord).r;
    }
    
    // Emission
    vec3 emission = u_EmissionColor * u_EmissionStrength;
    if (u_UseEmissionMap) {
        emission = texture(u_EmissionMap, fs_in.TexCoord).rgb * u_EmissionStrength;
    }
    
    // Encode normal to octahedron
    vec2 encodedNormal = encodeOctahedron(normal);
    
    // Output to G-Buffer
    gAlbedoMetallic = vec4(albedo, metallic);
    gNormalRoughness = vec4(encodedNormal, roughness, ao);
    gEmissionID = vec4(emission, u_MaterialID);
}
