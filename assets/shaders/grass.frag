#version 410 core

in vec2 v_texcoord;
in float v_color_variation;
in float v_world_height;
in vec3 v_world_pos;

out vec4 frag_color;

#include "common/scene_data.glsl"
#include "common/camera.glsl"
#include "common/fog.glsl"

uniform sampler2D u_grass_texture;
uniform vec3 u_base_color;
uniform vec3 u_tip_color;
// uniform vec3 u_view_pos; // In CameraData

void main() {
    // Sample grass texture
    vec4 tex_color = texture(u_grass_texture, v_texcoord);
    
    // Alpha test - discard transparent pixels
    if (tex_color.a < 0.5) {
        discard;
    }
    
    // Color gradient from base to tip
    float height_factor = 1.0 - v_texcoord.y; // 0 at bottom, 1 at top
    vec3 grass_color = mix(u_base_color, u_tip_color, height_factor);
    
    // Apply color variation for realism
    float variation = v_color_variation * 0.3; // Max 30% variation
    grass_color = mix(grass_color, grass_color * 0.7, variation);
    
    // Apply texture color (modulate)
    grass_color *= tex_color.rgb;
    
    // Simple lighting (grass has upward-facing normal)
    vec3 N = vec3(0.0, 1.0, 0.0);
    vec3 L = normalize(-u_sun.direction.xyz);
    float NdotL = max(dot(N, L), 0.0);
    
    // Combine lighting
    vec3 diffuse = u_sun.color.xyz * u_sun.intensity.x * NdotL * 0.8;
    vec3 ambient = u_ambient_light.xyz * 0.6;
    
    vec3 final_color = grass_color * (diffuse + ambient);
    
    // Apply Fog
    final_color = apply_fog(final_color, length(u_view_pos - v_world_pos));
    
    frag_color = vec4(final_color, tex_color.a);
}
