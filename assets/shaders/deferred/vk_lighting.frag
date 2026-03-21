#version 450

/**
 * Deferred Lighting Pass - Fragment Shader (Vulkan)
 *
 * Reads GBuffer textures and computes PBR lighting (Cook-Torrance BRDF).
 * Supports a directional sun light + up to 256 point lights via SSBO.
 * Outputs HDR color to a RGBA16F render target.
 */

layout(location = 0) in vec2 v_TexCoord;
layout(location = 0) out vec4 out_Color;

// Camera data (set 0, binding 0)
layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 projection;
    mat4 view_projection;
    vec4 camera_position; // xyz = pos, w = unused
} camera;

// GBuffer samplers (set 1)
layout(set = 1, binding = 0) uniform sampler2D u_GBufAlbedoMetallic;
layout(set = 1, binding = 1) uniform sampler2D u_GBufNormalRoughness;
layout(set = 1, binding = 2) uniform sampler2D u_GBufEmissionID;
layout(set = 1, binding = 3) uniform sampler2D u_GBufDepth;
layout(set = 1, binding = 4) uniform sampler2DArray u_ShadowMap;
layout(set = 1, binding = 5) uniform sampler2D u_SSAOTexture;

// Light data (set 2)
layout(set = 2, binding = 0) uniform LightUBO {
    vec4 sun_direction;   // xyz = direction (towards light), w = unused
    vec4 sun_color;       // xyz = color, w = intensity
    uvec4 light_counts;   // x = point light count, yzw = reserved
} lights;

struct GPUPointLight {
    vec4 position_radius;  // xyz = position, w = radius
    vec4 color_intensity;  // xyz = color, w = intensity
};

layout(set = 2, binding = 1) readonly buffer PointLightSSBO {
    GPUPointLight point_lights[];
};

layout(set = 2, binding = 2) uniform ShadowUBO {
    mat4 light_space_matrices[4];
    vec4 cascade_splits;
    vec4 params; // x=enabled, y=bias
} shadows;

// --------------------------------------------------------------------------
// Constants
// --------------------------------------------------------------------------
const float PI = 3.14159265359;
const float EPSILON = 0.0001;

// --------------------------------------------------------------------------
// Octahedron normal decoding [0,1] -> [-1,1]
// --------------------------------------------------------------------------
vec3 decode_octahedron(vec2 f) {
    f = f * 2.0 - 1.0;
    vec3 n = vec3(f.x, f.y, 1.0 - abs(f.x) - abs(f.y));
    float t = clamp(-n.z, 0.0, 1.0);
    n.x += n.x >= 0.0 ? -t : t;
    n.y += n.y >= 0.0 ? -t : t;
    return normalize(n);
}

// --------------------------------------------------------------------------
// World position reconstruction from depth
// --------------------------------------------------------------------------
vec3 reconstruct_world_pos(vec2 uv, float depth) {
    // NDC: xy in [-1,1], z in [0,1] (Vulkan)
    vec4 ndc = vec4(uv * 2.0 - 1.0, depth, 1.0);
    // Flip Y for Vulkan (clip space Y is inverted)
    ndc.y = -ndc.y;
    vec4 world = inverse(camera.view_projection) * ndc;
    return world.xyz / world.w;
}

// --------------------------------------------------------------------------
// Shadow Calculation (CSM + PCF)
// --------------------------------------------------------------------------
float shadow_calculation(vec3 world_pos, vec3 N, vec3 L) {
    vec4 view_pos = camera.view * vec4(world_pos, 1.0);
    float depth_value = abs(view_pos.z);

    int layer = -1;
    for (int i = 0; i < 4; ++i) {
        if (depth_value < shadows.cascade_splits[i]) {
            layer = i;
            break;
        }
    }
    if (layer == -1) layer = 3;

    vec4 light_space_pos = shadows.light_space_matrices[layer] * vec4(world_pos, 1.0);
    vec3 proj_coords = light_space_pos.xyz / light_space_pos.w;
    
    // Vulkan UV: [0,1], Z: [0,1]
    // CSM matrices usually map to [0,1] already if configured for Vulkan clip space
    // Assuming standard ortho mapping [-1,1] -> [0,1] xy
    proj_coords.xy = proj_coords.xy * 0.5 + 0.5;

    // Check depth range
    if (proj_coords.z > 1.0) return 0.0;

    // Bias
    float bias = max(shadows.params.y * (1.0 - dot(N, L)), 0.0005);
    
    // PCF
    float shadow = 0.0;
    vec2 texel_size = 1.0 / textureSize(u_ShadowMap, 0).xy;
    for(int x = -1; x <= 1; ++x) {
        for(int y = -1; y <= 1; ++y) {
            float pcf_depth = texture(u_ShadowMap, vec3(proj_coords.xy + vec2(x, y) * texel_size, layer)).r; 
            shadow += (proj_coords.z - bias > pcf_depth ? 1.0 : 0.0);        
        }    
    }
    shadow /= 9.0;
    
    return shadow;
}

// --------------------------------------------------------------------------
// PBR: GGX/Trowbridge-Reitz Normal Distribution Function
// --------------------------------------------------------------------------
float distribution_ggx(vec3 N, vec3 H, float roughness) {
    float a  = roughness * roughness;
    float a2 = a * a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float denom = NdotH2 * (a2 - 1.0) + 1.0;
    denom = PI * denom * denom;
    return a2 / max(denom, EPSILON);
}

// --------------------------------------------------------------------------
// PBR: Smith's Geometry Function (Schlick-GGX)
// --------------------------------------------------------------------------
float geometry_schlick_ggx(float NdotV, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0; // Direct lighting k
    return NdotV / (NdotV * (1.0 - k) + k);
}

float geometry_smith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    return geometry_schlick_ggx(NdotV, roughness) * geometry_schlick_ggx(NdotL, roughness);
}

// --------------------------------------------------------------------------
// PBR: Fresnel-Schlick
// --------------------------------------------------------------------------
vec3 fresnel_schlick(float cos_theta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cos_theta, 0.0, 1.0), 5.0);
}

// --------------------------------------------------------------------------
// Cook-Torrance BRDF evaluation for a single light
// --------------------------------------------------------------------------
vec3 evaluate_light(vec3 N, vec3 V, vec3 L, vec3 light_color, float light_intensity,
                    vec3 albedo, float metallic, float roughness, vec3 F0) {
    vec3 H = normalize(V + L);

    float NdotL = max(dot(N, L), 0.0);
    if (NdotL <= 0.0) return vec3(0.0);

    // Cook-Torrance specular BRDF
    float D = distribution_ggx(N, H, roughness);
    float G = geometry_smith(N, V, L, roughness);
    vec3  F = fresnel_schlick(max(dot(H, V), 0.0), F0);

    vec3 numerator = D * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * NdotL + EPSILON;
    vec3 specular = numerator / denominator;

    // Energy conservation: diffuse = 1 - specular (kS)
    vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);

    // Lambertian diffuse
    vec3 diffuse = kD * albedo / PI;

    return (diffuse + specular) * light_color * light_intensity * NdotL;
}

void main() {
    // Sample GBuffer
    vec4 albedo_metallic   = texture(u_GBufAlbedoMetallic, v_TexCoord);
    vec4 normal_roughness  = texture(u_GBufNormalRoughness, v_TexCoord);
    vec4 emission_id       = texture(u_GBufEmissionID, v_TexCoord);
    float depth            = texture(u_GBufDepth, v_TexCoord).r;

    // Early out for sky (depth == 1.0 means nothing was written)
    if (depth >= 1.0) {
        out_Color = vec4(0.05, 0.05, 0.08, 1.0); // Sky color placeholder
        return;
    }

    // Unpack GBuffer
    vec3  albedo    = albedo_metallic.rgb;
    float metallic  = albedo_metallic.a;
    vec3  N         = decode_octahedron(normal_roughness.rg);
    float roughness = max(normal_roughness.b, 0.04); // Clamp roughness to avoid divide-by-zero
    float ao        = normal_roughness.a;
    vec3  emission  = emission_id.rgb;

    // Reconstruct world position
    vec3 world_pos = reconstruct_world_pos(v_TexCoord, depth);

    // View direction
    vec3 V = normalize(camera.camera_position.xyz - world_pos);

    // Base reflectivity (F0)
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    // Accumulate lighting
    vec3 Lo = vec3(0.0);

    // Directional sun light
    {
        vec3  L = normalize(lights.sun_direction.xyz);
        float intensity = lights.sun_color.w;
        
        float shadow = 0.0;
        if (shadows.params.x > 0.5) {
             shadow = shadow_calculation(world_pos, N, L);
        }

        Lo += (1.0 - shadow) * evaluate_light(N, V, L, lights.sun_color.rgb, intensity,
                             albedo, metallic, roughness, F0);
    }

    // Point lights
    uint point_light_count = min(lights.light_counts.x, 256u);
    for (uint i = 0u; i < point_light_count; ++i) {
        vec3  lpos = point_lights[i].position_radius.xyz;
        float lradius = point_lights[i].position_radius.w;
        vec3  lcolor = point_lights[i].color_intensity.rgb;
        float lintensity = point_lights[i].color_intensity.w;

        vec3 L_vec = lpos - world_pos;
        float dist = length(L_vec);

        // Skip lights out of range
        if (dist > lradius) continue;

        vec3 L = L_vec / dist;

        // Smooth distance attenuation (UE4-style)
        float dist_ratio = dist / lradius;
        float falloff = clamp(1.0 - dist_ratio * dist_ratio * dist_ratio * dist_ratio, 0.0, 1.0);
        falloff = falloff * falloff;
        float attenuation = falloff / (dist * dist + 1.0);

        Lo += evaluate_light(N, V, L, lcolor, lintensity * attenuation,
                             albedo, metallic, roughness, F0);
    }

    // Ambient approximation with SSAO modulation.
    vec3 ambient = vec3(0.03) * albedo * ao;
    ambient *= texture(u_SSAOTexture, v_TexCoord).r;

    vec3 color = ambient + Lo + emission;

    out_Color = vec4(color, 1.0);
}
