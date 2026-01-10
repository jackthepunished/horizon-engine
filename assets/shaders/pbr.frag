#version 410 core

out vec4 frag_color;

in vec3 v_world_pos;
in vec2 v_texcoord;
in vec4 v_frag_pos_light_space;
in mat3 v_TBN;

// PBR Textures
uniform sampler2D u_albedo_map;
uniform sampler2D u_normal_map;
uniform sampler2D u_metallic_map;
uniform sampler2D u_roughness_map;
uniform sampler2D u_ao_map;

uniform sampler2D u_shadow_map;
uniform sampler2D u_ssao_map;

// IBL Textures
uniform samplerCube u_irradiance_map;
uniform samplerCube u_prefilter_map;
uniform sampler2D u_brdf_lut;
uniform bool u_use_ibl;

// Toggles (for missing textures)
uniform bool u_use_normal_map;
uniform bool u_use_metallic_map;
uniform bool u_use_roughness_map;
uniform bool u_use_ao_map;

// Fallback values
uniform vec3 u_albedo;
uniform float u_metallic;
uniform float u_roughness;
uniform float u_ao;
uniform float u_uv_scale;
uniform bool u_use_textures;

// Camera
uniform vec3 u_cam_pos;
uniform vec3 u_view_pos;
uniform vec2 u_viewport_size;

// Lights
struct DirectionalLight {
    vec3 direction;
    vec3 color;
    float intensity;
};
uniform DirectionalLight u_sun;
uniform vec3 u_ambient_light;

#define MAX_POINT_LIGHTS 16
struct PointLight {
    vec3 position;
    vec3 color;
    float intensity;
    float range;
};
uniform int u_point_light_count;
uniform PointLight u_point_lights[MAX_POINT_LIGHTS];

const float PI = 3.14159265359;
const float MAX_REFLECTION_LOD = 4.0;

// ============================================
// PBR Functions
// ============================================

// Normal Distribution Function (GGX/Trowbridge-Reitz)
float distribution_ggx(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    
    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    
    return num / denom;
}

// Geometry Function (Schlick-GGX)
float geometry_schlick_ggx(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    
    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    
    return num / denom;
}

// Smith's method for geometry
float geometry_smith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = geometry_schlick_ggx(NdotV, roughness);
    float ggx1 = geometry_schlick_ggx(NdotL, roughness);
    
    return ggx1 * ggx2;
}

// Fresnel (Schlick approximation)
vec3 fresnel_schlick(float cos_theta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cos_theta, 0.0, 1.0), 5.0);
}

// Fresnel with roughness for IBL
vec3 fresnel_schlick_roughness(float cos_theta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cos_theta, 0.0, 1.0), 5.0);
}

// Calculate Shadow
float calculate_shadow(vec4 frag_pos_light_space, vec3 N, vec3 L) {
    // perform perspective divide
    vec3 proj_coords = frag_pos_light_space.xyz / frag_pos_light_space.w;
    
    // transform to [0,1] range
    proj_coords = proj_coords * 0.5 + 0.5;
    
    // keep the shadow at 0.0 when outside the far_plane region of the light's frustum.
    if(proj_coords.z > 1.0)
        return 0.0;
        
    // Calculate bias (based on depth map resolution and slope)
    float bias = max(0.05 * (1.0 - dot(N, L)), 0.005);
    
    // PCF (Percentage-closer filtering)
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(u_shadow_map, 0);
    for(int x = -1; x <= 1; ++x) {
        for(int y = -1; y <= 1; ++y) {
            float pcfDepth = texture(u_shadow_map, proj_coords.xy + vec2(x, y) * texelSize).r; 
            shadow += (proj_coords.z - bias > pcfDepth) ? 1.0 : 0.0;        
        }    
    }
    shadow /= 9.0;
    
    return shadow;
}

// Calculate lighting for a single light
vec3 calculate_light(vec3 L, vec3 radiance, vec3 N, vec3 V, vec3 albedo, 
                     float metallic, float roughness, vec3 F0) {
    vec3 H = normalize(V + L);
    
    // Cook-Torrance BRDF
    float NDF = distribution_ggx(N, H, roughness);
    float G = geometry_smith(N, V, L, roughness);
    vec3 F = fresnel_schlick(max(dot(H, V), 0.0), F0);
    
    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic; // Metals have no diffuse
    
    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3 specular = numerator / denominator;
    
    float NdotL = max(dot(N, L), 0.0);
    return (kD * albedo / PI + specular) * radiance * NdotL;
}

void main() {
    // UV scaling
    vec2 scaled_uv = v_texcoord * u_uv_scale;
    
    // Sample textures or use fallback values
    vec3 albedo;
    if (u_use_textures) {
        albedo = pow(texture(u_albedo_map, scaled_uv).rgb, vec3(2.2)); // sRGB to linear
    } else {
        albedo = u_albedo;
    }
    
    float metallic = u_use_metallic_map ? texture(u_metallic_map, scaled_uv).r : u_metallic;
    float roughness = u_use_roughness_map ? texture(u_roughness_map, scaled_uv).r : u_roughness;
    roughness = max(roughness, 0.04); // Prevent divide by zero in specular
    float ao = u_use_ao_map ? texture(u_ao_map, scaled_uv).r : u_ao;
    
    // Normal
    vec3 N;
    if (u_use_normal_map) {
        N = texture(u_normal_map, scaled_uv).rgb * 2.0 - 1.0;
        N = normalize(v_TBN * N);
    } else {
        N = normalize(v_TBN[2]);
    }
    
    // View direction - use whichever uniform is set
    vec3 cam_pos = length(u_cam_pos) > 0.0 ? u_cam_pos : u_view_pos;
    vec3 V = normalize(cam_pos - v_world_pos);
    vec3 R = reflect(-V, N);
    
    // Fresnel reflectance at normal incidence
    vec3 F0 = vec3(0.04); // Dielectric
    F0 = mix(F0, albedo, metallic); // Metals use albedo as F0
    
    vec3 Lo = vec3(0.0);
    
    // Sun (Directional Light)
    vec3 L_sun = normalize(-u_sun.direction);
    vec3 sun_radiance = u_sun.color * u_sun.intensity;
    float shadow = calculate_shadow(v_frag_pos_light_space, N, L_sun);
    Lo += (1.0 - shadow) * calculate_light(L_sun, sun_radiance, N, V, albedo, metallic, roughness, F0);
    
    // Point lights
    for (int i = 0; i < u_point_light_count; ++i) {
        vec3 L = normalize(u_point_lights[i].position - v_world_pos);
        float distance = length(u_point_lights[i].position - v_world_pos);
        float attenuation = clamp(1.0 - (distance / u_point_lights[i].range), 0.0, 1.0);
        attenuation *= attenuation;
        vec3 radiance = u_point_lights[i].color * u_point_lights[i].intensity * attenuation;
        Lo += calculate_light(L, radiance, N, V, albedo, metallic, roughness, F0);
    }
    
    // Ambient lighting
    vec3 ambient;
    
    if (u_use_ibl) {
        // IBL Ambient
        vec3 F = fresnel_schlick_roughness(max(dot(N, V), 0.0), F0, roughness);
        
        vec3 kS = F;
        vec3 kD = 1.0 - kS;
        kD *= 1.0 - metallic;
        
        // Diffuse IBL
        vec3 irradiance = texture(u_irradiance_map, N).rgb;
        vec3 diffuse = irradiance * albedo;
        
        // Specular IBL
        vec3 prefiltered_color = textureLod(u_prefilter_map, R, roughness * MAX_REFLECTION_LOD).rgb;
        vec2 brdf = texture(u_brdf_lut, vec2(max(dot(N, V), 0.0), roughness)).rg;
        vec3 specular = prefiltered_color * (F * brdf.x + brdf.y);
        
        ambient = (kD * diffuse + specular) * ao;
    } else {
        // Simple ambient fallback
        ambient = u_ambient_light * albedo * ao;
    }
    
    // SSAO
    vec2 screen_uv = gl_FragCoord.xy / u_viewport_size;
    float ssao_factor = texture(u_ssao_map, screen_uv).r;
    ambient *= ssao_factor;
    
    vec3 color = ambient + Lo;
    
    frag_color = vec4(color, 1.0);
}
