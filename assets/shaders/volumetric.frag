#version 410 core

out vec4 frag_color;

in vec2 v_texcoord;

// Scene textures
uniform sampler2D u_scene_texture;
uniform sampler2D u_depth_texture;
uniform sampler2D u_shadow_map;

// Camera uniforms
uniform mat4 u_inverse_view_projection;
uniform vec3 u_camera_pos;
uniform float u_near_plane;
uniform float u_far_plane;

// Light uniforms
uniform vec3 u_light_dir;       // Direction TO light (normalized)
uniform vec3 u_light_color;
uniform float u_light_intensity;
uniform mat4 u_light_space_matrix;

// Volumetric settings
uniform float u_fog_density;        // Base fog density
uniform float u_fog_height_falloff; // How quickly fog fades with height
uniform float u_fog_base_height;    // Height below which fog is densest
uniform vec3 u_fog_color;           // Fog/scattering color
uniform float u_scattering_coeff;   // Light scattering coefficient
uniform float u_absorption_coeff;   // Light absorption coefficient

// God rays settings
uniform float u_god_ray_intensity;  // Intensity of god rays
uniform float u_god_ray_decay;      // How quickly rays fade
uniform int u_ray_march_steps;      // Number of ray march steps (16-64)

// Noise for variation
uniform float u_time;

const float PI = 3.14159265359;

// ============================================
// Utility Functions
// ============================================

// Linearize depth from depth buffer
float linearize_depth(float depth) {
    float z = depth * 2.0 - 1.0; // Back to NDC
    return (2.0 * u_near_plane * u_far_plane) / (u_far_plane + u_near_plane - z * (u_far_plane - u_near_plane));
}

// Reconstruct world position from depth
vec3 world_pos_from_depth(vec2 uv, float depth) {
    vec4 clip_pos = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 world_pos = u_inverse_view_projection * clip_pos;
    return world_pos.xyz / world_pos.w;
}

// Simple hash for noise
float hash(vec3 p) {
    p = fract(p * 0.3183099 + 0.1);
    p *= 17.0;
    return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}

// 3D noise for fog variation
float noise3d(vec3 p) {
    vec3 i = floor(p);
    vec3 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    
    return mix(
        mix(mix(hash(i + vec3(0,0,0)), hash(i + vec3(1,0,0)), f.x),
            mix(hash(i + vec3(0,1,0)), hash(i + vec3(1,1,0)), f.x), f.y),
        mix(mix(hash(i + vec3(0,0,1)), hash(i + vec3(1,0,1)), f.x),
            mix(hash(i + vec3(0,1,1)), hash(i + vec3(1,1,1)), f.x), f.y),
        f.z
    );
}

// Fractal Brownian Motion for detailed fog
float fbm(vec3 p) {
    float value = 0.0;
    float amplitude = 0.5;
    float frequency = 1.0;
    
    for (int i = 0; i < 4; i++) {
        value += amplitude * noise3d(p * frequency);
        amplitude *= 0.5;
        frequency *= 2.0;
    }
    return value;
}

// ============================================
// Fog Density Functions
// ============================================

// Calculate fog density at a point
float get_fog_density(vec3 pos) {
    // Height-based exponential falloff
    float height_factor = exp(-max(0.0, pos.y - u_fog_base_height) * u_fog_height_falloff);
    
    // Add noise variation for more natural look
    vec3 noise_pos = pos * 0.05 + vec3(u_time * 0.1, 0.0, u_time * 0.05);
    float noise_value = fbm(noise_pos) * 0.5 + 0.5;
    
    // Combine base density with height falloff and noise
    float density = u_fog_density * height_factor * noise_value;
    
    return max(0.0, density);
}

// ============================================
// Shadow Functions
// ============================================

// Check if a point is in shadow
float get_shadow(vec3 world_pos) {
    vec4 light_space_pos = u_light_space_matrix * vec4(world_pos, 1.0);
    vec3 proj_coords = light_space_pos.xyz / light_space_pos.w;
    proj_coords = proj_coords * 0.5 + 0.5;
    
    // Out of shadow map bounds
    if (proj_coords.x < 0.0 || proj_coords.x > 1.0 ||
        proj_coords.y < 0.0 || proj_coords.y > 1.0 ||
        proj_coords.z > 1.0) {
        return 0.0; // Not in shadow
    }
    
    float shadow_depth = texture(u_shadow_map, proj_coords.xy).r;
    float current_depth = proj_coords.z;
    float bias = 0.005;
    
    return current_depth - bias > shadow_depth ? 1.0 : 0.0;
}

// ============================================
// Phase Functions (Light Scattering)
// ============================================

// Henyey-Greenstein phase function for anisotropic scattering
// g: asymmetry parameter (-1 to 1), positive = forward scattering
float henyey_greenstein(float cos_theta, float g) {
    float g2 = g * g;
    float denom = 1.0 + g2 - 2.0 * g * cos_theta;
    return (1.0 - g2) / (4.0 * PI * pow(denom, 1.5));
}

// Mie scattering approximation for god rays
float mie_scattering(float cos_theta) {
    // Combine forward and back scattering
    float forward = henyey_greenstein(cos_theta, 0.7);  // Strong forward scatter
    float back = henyey_greenstein(cos_theta, -0.3);    // Weak back scatter
    return mix(back, forward, 0.8);
}

// ============================================
// Main Ray Marching
// ============================================

void main() {
    // Sample scene color and depth
    vec3 scene_color = texture(u_scene_texture, v_texcoord).rgb;
    float depth = texture(u_depth_texture, v_texcoord).r;
    
    // Skip sky pixels (depth = 1.0)
    if (depth >= 0.9999) {
        // Still apply some atmospheric scattering to sky
        vec3 ray_dir = normalize(world_pos_from_depth(v_texcoord, 0.5) - u_camera_pos);
        float cos_theta = dot(ray_dir, u_light_dir);
        float sky_scatter = mie_scattering(cos_theta) * u_god_ray_intensity * 0.3;
        vec3 sky_fog = u_fog_color * sky_scatter * u_light_color;
        frag_color = vec4(scene_color + sky_fog, 1.0);
        return;
    }
    
    // Reconstruct world position
    vec3 world_pos = world_pos_from_depth(v_texcoord, depth);
    
    // Ray from camera to world position
    vec3 ray_origin = u_camera_pos;
    vec3 ray_end = world_pos;
    vec3 ray_dir = normalize(ray_end - ray_origin);
    float ray_length = length(ray_end - ray_origin);
    
    // Limit ray march distance for performance
    float max_distance = min(ray_length, 200.0);
    float step_size = max_distance / float(u_ray_march_steps);
    
    // Ray marching variables
    vec3 accumulated_fog = vec3(0.0);
    float transmittance = 1.0;
    
    // Jitter starting position to reduce banding
    float jitter = hash(vec3(v_texcoord * 1000.0, u_time)) * step_size;
    
    // Phase function for current view direction
    float cos_theta = dot(ray_dir, u_light_dir);
    float phase = mie_scattering(cos_theta);
    
    // Ray march through the volume
    for (int i = 0; i < u_ray_march_steps; i++) {
        float t = jitter + float(i) * step_size;
        if (t > max_distance) break;
        
        vec3 sample_pos = ray_origin + ray_dir * t;
        
        // Get fog density at this point
        float density = get_fog_density(sample_pos);
        
        if (density > 0.001) {
            // Check shadow
            float shadow = get_shadow(sample_pos);
            float light_visibility = 1.0 - shadow;
            
            // Calculate in-scattering (light scattered towards camera)
            vec3 in_scatter = u_light_color * u_light_intensity * phase * 
                             u_scattering_coeff * light_visibility;
            
            // Add ambient fog color
            vec3 ambient_fog = u_fog_color * 0.2;
            
            // Total light at this sample
            vec3 sample_light = (in_scatter + ambient_fog) * density;
            
            // Beer-Lambert law for extinction
            float extinction = (u_scattering_coeff + u_absorption_coeff) * density * step_size;
            float sample_transmittance = exp(-extinction);
            
            // Accumulate fog with proper integration
            accumulated_fog += sample_light * transmittance * step_size;
            transmittance *= sample_transmittance;
            
            // Early exit if fully opaque
            if (transmittance < 0.01) break;
        }
    }
    
    // God rays enhancement - add extra brightness along light direction
    float god_ray_factor = pow(max(0.0, cos_theta), 8.0) * u_god_ray_intensity;
    accumulated_fog += u_light_color * god_ray_factor * (1.0 - transmittance) * 0.5;
    
    // Composite fog with scene
    vec3 final_color = scene_color * transmittance + accumulated_fog;
    
    frag_color = vec4(final_color, 1.0);
}
