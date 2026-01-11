#version 410 core

out vec4 frag_color;

in vec3 v_world_pos;
in vec3 v_normal;
in vec2 v_texcoord;
in vec2 v_splatcoord;
in vec4 v_frag_pos_light_space;

// Splatmap (RGBA = 4 texture weights)
uniform sampler2D u_splatmap;

// Detail textures (albedo)
uniform sampler2D u_texture0; // R channel - e.g., Grass
uniform sampler2D u_texture1; // G channel - e.g., Rock
uniform sampler2D u_texture2; // B channel - e.g., Dirt
uniform sampler2D u_texture3; // A channel - e.g., Sand

// Normal maps (optional)
uniform sampler2D u_normal0;
uniform sampler2D u_normal1;
uniform sampler2D u_normal2;
uniform sampler2D u_normal3;
uniform bool u_use_normal_maps;

// Shadow map
uniform sampler2D u_shadow_map;

// IBL
uniform samplerCube u_irradiance_map;
uniform samplerCube u_prefilter_map;
uniform sampler2D u_brdf_lut;
uniform bool u_use_ibl;

// Lighting
// Lights
// Maps
#include "common/scene_data.glsl"

// Math & PBR
#include "common/math.glsl"
#include "common/pbr_functions.glsl"

// Fog
#include "common/fog.glsl"

// Camera
#include "common/camera.glsl"

// Material properties
uniform float u_roughness;
uniform float u_metallic;
uniform bool u_use_splatmap;

// PBR Maps (New)
uniform sampler2D u_roughness_map;
uniform sampler2D u_ao_map;
uniform bool u_use_pbr_maps;

// ============================================
// Helper Functions
// ============================================

float calculate_shadow(vec4 frag_pos_light_space, vec3 N, vec3 L) {
    vec3 proj_coords = frag_pos_light_space.xyz / frag_pos_light_space.w;
    proj_coords = proj_coords * 0.5 + 0.5;
    
    if(proj_coords.z > 1.0)
        return 0.0;
        
    float bias = max(0.05 * (1.0 - dot(N, L)), 0.005);
    
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

void main() {
    vec3 albedo;
    vec4 splat = vec4(1.0, 0.0, 0.0, 0.0); // Default: use texture0 only
    
    if (u_use_splatmap) {
        // Sample splatmap
        splat = texture(u_splatmap, v_splatcoord);
        
        // Normalize weights (in case they don't sum to 1)
        float total_weight = splat.r + splat.g + splat.b + splat.a;
        if (total_weight > 0.0) {
            splat /= total_weight;
        } else {
            splat = vec4(1.0, 0.0, 0.0, 0.0); // Default to first texture
        }
        
        // Sample detail textures and blend
        vec3 color0 = texture(u_texture0, v_texcoord).rgb;
        vec3 color1 = texture(u_texture1, v_texcoord).rgb;
        vec3 color2 = texture(u_texture2, v_texcoord).rgb;
        vec3 color3 = texture(u_texture3, v_texcoord).rgb;
        
        albedo = color0 * splat.r + color1 * splat.g + color2 * splat.b + color3 * splat.a;
    } else {
        // No splatmap - use texture0 directly
        albedo = texture(u_texture0, v_texcoord).rgb;
    }
    
    albedo = pow(albedo, vec3(2.2)); // sRGB to linear
    
    // Normal
    vec3 N = normalize(v_normal);
    
    if (u_use_normal_maps) {
        vec3 n0 = texture(u_normal0, v_texcoord).rgb * 2.0 - 1.0;
        vec3 n1 = texture(u_normal1, v_texcoord).rgb * 2.0 - 1.0;
        vec3 n2 = texture(u_normal2, v_texcoord).rgb * 2.0 - 1.0;
        vec3 n3 = texture(u_normal3, v_texcoord).rgb * 2.0 - 1.0;
        
        vec3 blended_normal = n0 * splat.r + n1 * splat.g + n2 * splat.b + n3 * splat.a;
        
        // Simple tangent space (terrain is mostly horizontal)
        vec3 T = normalize(vec3(1.0, 0.0, 0.0));
        vec3 B = normalize(cross(N, T));
        T = cross(B, N);
        mat3 TBN = mat3(T, B, N);
        
        N = normalize(TBN * blended_normal);
    }
    
    vec3 V = normalize(u_view_pos - v_world_pos);
    vec3 R = reflect(-V, N);
    
    float roughness = u_roughness;
    float metallic = u_metallic;
    float ao = 1.0;

    if (u_use_pbr_maps) {
        // Sample PBR maps (assuming consistent UVs with albedo)
        roughness = texture(u_roughness_map, v_texcoord).r;
        ao = texture(u_ao_map, v_texcoord).r;
    }
    
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);
    
    vec3 Lo = vec3(0.0);
    
    // Sun light
    vec3 L = normalize(-u_sun.direction.xyz);
    vec3 H = normalize(V + L);
    vec3 radiance = u_sun.color.xyz * u_sun.intensity.x;
    
    float NDF = distribution_ggx(N, H, roughness);
    float G = geometry_smith(N, V, L, roughness);
    vec3 F = fresnel_schlick(max(dot(H, V), 0.0), F0);
    
    vec3 kS = F;
    vec3 kD = (1.0 - kS) * (1.0 - metallic);
    
    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3 specular = numerator / denominator;
    
    float NdotL = max(dot(N, L), 0.0);
    float shadow = calculate_shadow(v_frag_pos_light_space, N, L);
    Lo += (1.0 - shadow) * (kD * albedo / PI + specular) * radiance * NdotL;
    
    // Point Lights
    for (int i = 0; i < u_point_light_count; ++i) {
        vec3 L_point = normalize(u_point_lights[i].position.xyz - v_world_pos);
        vec3 H_point = normalize(V + L_point);
        
        float distance = length(u_point_lights[i].position.xyz - v_world_pos);
        float attenuation = clamp(1.0 - (distance / u_point_lights[i].range), 0.0, 1.0);
        attenuation *= attenuation; // Quadratic falloff
        
        vec3 point_radiance = u_point_lights[i].color.xyz * u_point_lights[i].intensity * attenuation;
        
        float NDF_p = distribution_ggx(N, H_point, roughness);
        float G_p = geometry_smith(N, V, L_point, roughness);
        vec3 F_p = fresnel_schlick(max(dot(H_point, V), 0.0), F0);
        
        vec3 kS_p = F_p;
        vec3 kD_p = (1.0 - kS_p) * (1.0 - metallic);
        
        vec3 num_p = NDF_p * G_p * F_p;
        float denom_p = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L_point), 0.0) + 0.0001;
        vec3 spec_p = num_p / denom_p;
        
        float NdotL_p = max(dot(N, L_point), 0.0);
        Lo += (kD_p * albedo / PI + spec_p) * point_radiance * NdotL_p;
    }
    
    // Ambient
    vec3 ambient;
    if (u_use_ibl) {
        vec3 F_ibl = fresnel_schlick_roughness(max(dot(N, V), 0.0), F0, roughness);
        vec3 kS_ibl = F_ibl;
        vec3 kD_ibl = (1.0 - kS_ibl) * (1.0 - metallic);
        
        vec3 irradiance = texture(u_irradiance_map, N).rgb;
        vec3 diffuse = irradiance * albedo;
        
        vec3 prefiltered = textureLod(u_prefilter_map, R, roughness * MAX_REFLECTION_LOD).rgb;
        vec2 brdf = texture(u_brdf_lut, vec2(max(dot(N, V), 0.0), roughness)).rg;
        vec3 spec = prefiltered * (F_ibl * brdf.x + brdf.y);
        
        ambient = kD_ibl * diffuse + spec;
    } else {
        ambient = u_ambient_light.xyz * albedo;
    }
    
    // Apply Ambient Occlusion
    ambient *= ao;
    
    vec3 color = ambient + Lo;
    
    // Apply Fog
    color = apply_fog(color, length(u_view_pos - v_world_pos));
    
    frag_color = vec4(color, 1.0);
}
