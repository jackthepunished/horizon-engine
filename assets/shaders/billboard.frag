#version 410 core

out vec4 frag_color;

in vec2 v_texcoord;
in vec4 v_color;
in vec3 v_world_pos;

#include "common/camera.glsl"
#include "common/scene_data.glsl"

uniform sampler2D u_texture;
uniform bool u_use_texture;

void main() {
    vec4 tex_color = u_use_texture ? texture(u_texture, v_texcoord) : vec4(1.0);
    
    // Alpha test - discard transparent pixels
    if (tex_color.a < 0.5) {
        discard;
    }
    
    // Apply instance tint color - preserve original texture colors
    vec3 color = tex_color.rgb * v_color.rgb;
    
    // Very subtle lighting - billboards should mostly show their texture
    vec3 light_dir = normalize(-u_sun.direction.xyz);
    float ndotl = max(dot(vec3(0.0, 1.0, 0.0), light_dir), 0.0);
    float light = 0.7 + 0.3 * ndotl; // High ambient, subtle diffuse
    
    // Apply gentle lighting without over-darkening
    color *= light;
    
    // Add subtle ambient contribution
    color += u_ambient_light.rgb * 0.15;
    
    // Very subtle distance fog for depth
    float dist = length(v_world_pos - u_view_pos.xyz);
    float fog_factor = 1.0 - exp(-dist * u_fog_density * 0.3);
    fog_factor = clamp(fog_factor, 0.0, 0.4); // Max 40% fog
    color = mix(color, u_fog_color.rgb, fog_factor * float(u_fog_enabled));
    
    frag_color = vec4(color, tex_color.a * v_color.a);
}
