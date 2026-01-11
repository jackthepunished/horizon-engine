#version 410 core

out vec4 frag_color;

in vec3 v_world_pos;
in vec2 v_texcoord;
in vec4 v_clip_space;
in vec3 v_to_camera;

#include "common/camera.glsl"
#include "common/scene_data.glsl"

// Water textures
uniform sampler2D u_reflection_texture;  // Reflection of scene
uniform sampler2D u_refraction_texture;  // Refraction (underwater view)
uniform sampler2D u_dudv_map;            // Distortion map
uniform sampler2D u_normal_map;          // Water normal map
uniform sampler2D u_depth_texture;       // Scene depth for soft edges

// Water properties (u_time is from SceneData UBO)
uniform vec3 u_water_color;        // Deep water color
uniform vec3 u_water_color_shallow; // Shallow water color
uniform float u_wave_speed;
uniform float u_distortion_strength;
uniform float u_shine_damper;      // Specular power
uniform float u_reflectivity;      // Specular strength
uniform float u_transparency;      // Water transparency
uniform float u_depth_multiplier;  // Depth effect strength

const float NEAR_PLANE = 0.1;
const float FAR_PLANE = 1000.0;

// Convert depth buffer value to linear depth
float linearize_depth(float depth) {
    return 2.0 * NEAR_PLANE * FAR_PLANE / (FAR_PLANE + NEAR_PLANE - (2.0 * depth - 1.0) * (FAR_PLANE - NEAR_PLANE));
}

void main() {
    // Normalized device coordinates for texture sampling
    vec2 ndc = (v_clip_space.xy / v_clip_space.w) * 0.5 + 0.5;
    vec2 refract_coords = ndc;
    vec2 reflect_coords = vec2(ndc.x, 1.0 - ndc.y);
    
    // Animated texture coordinates for waves
    float move_factor = u_time * u_wave_speed * 0.03;
    vec2 distorted_texcoords = v_texcoord * 4.0;
    distorted_texcoords.x += move_factor;
    
    // Sample DuDv map for distortion
    vec2 distortion1 = texture(u_dudv_map, distorted_texcoords).rg * 2.0 - 1.0;
    distorted_texcoords.y += move_factor;
    vec2 distortion2 = texture(u_dudv_map, vec2(-distorted_texcoords.x, distorted_texcoords.y + move_factor)).rg * 2.0 - 1.0;
    vec2 total_distortion = (distortion1 + distortion2) * u_distortion_strength;
    
    // Apply distortion to texture coordinates
    refract_coords += total_distortion;
    refract_coords = clamp(refract_coords, 0.001, 0.999);
    
    reflect_coords += total_distortion;
    reflect_coords = clamp(reflect_coords, 0.001, 0.999);
    
    // Sample reflection and refraction
    vec4 reflect_color = texture(u_reflection_texture, reflect_coords);
    vec4 refract_color = texture(u_refraction_texture, refract_coords);
    
    // Calculate water depth for soft edges and color blending
    float scene_depth = linearize_depth(texture(u_depth_texture, ndc).r);
    float water_depth = linearize_depth(gl_FragCoord.z);
    float depth_diff = scene_depth - water_depth;
    float depth_factor = clamp(depth_diff * u_depth_multiplier, 0.0, 1.0);
    
    // Blend between shallow and deep water color
    vec3 water_tint = mix(u_water_color_shallow, u_water_color, depth_factor);
    
    // Sample normal map for lighting
    vec2 normal_coords = distorted_texcoords * 0.5;
    vec4 normal_sample = texture(u_normal_map, normal_coords);
    vec3 normal = normalize(vec3(normal_sample.r * 2.0 - 1.0, normal_sample.b * 3.0, normal_sample.g * 2.0 - 1.0));
    
    // Fresnel effect - more reflection at grazing angles
    vec3 view_dir = normalize(v_to_camera);
    float fresnel = dot(view_dir, vec3(0.0, 1.0, 0.0));
    fresnel = pow(fresnel, 0.5);
    fresnel = clamp(fresnel, 0.0, 1.0);
    
    // Mix reflection and refraction based on fresnel
    vec4 water_surface = mix(reflect_color, refract_color, fresnel);
    
    // Add water tint
    water_surface.rgb = mix(water_surface.rgb, water_tint, 0.3);
    
    // Specular highlights from sun
    vec3 light_dir = normalize(-u_sun.direction.xyz);
    vec3 reflect_dir = reflect(-light_dir, normal);
    float spec = max(dot(reflect_dir, view_dir), 0.0);
    spec = pow(spec, u_shine_damper);
    vec3 specular = u_sun.color.xyz * u_sun.intensity.x * spec * u_reflectivity;
    
    // Final color
    vec3 final_color = water_surface.rgb + specular;
    
    // Soft edges based on depth
    float edge_softness = clamp(depth_diff * 5.0, 0.0, 1.0);
    float alpha = u_transparency * edge_softness;
    
    frag_color = vec4(final_color, alpha);
}
