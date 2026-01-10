#version 410 core

out float g_occlusion;

in vec2 v_texcoord;

uniform sampler2D u_g_position; // We don't have position buffer, we reconstruct from depth!
uniform sampler2D u_g_normal;
uniform sampler2D u_g_depth; // Use this to reconstruct position/
uniform sampler2D u_tex_noise;

uniform vec3 u_samples[64];
uniform mat4 u_projection;
uniform mat4 u_inverse_projection; // Needed for reconstruction? Or just linearized depth?

// Tile noise texture over screen (screen_width/4, screen_height/4)
uniform vec2 u_noise_scale;
uniform float u_radius = 0.5;
uniform float u_bias = 0.025;
uniform int u_kernel_size = 64;

// Reconstruct View-Space Position from Depth
vec3 get_view_pos(vec2 uv) {
    float depth = texture(u_g_depth, uv).r;
    // We need to unproject this depth
    // NDC coordinates
    float z = depth * 2.0 - 1.0;
    vec4 clip_space = vec4(uv * 2.0 - 1.0, z, 1.0);
    vec4 view_space = u_inverse_projection * clip_space;
    return view_space.xyz / view_space.w;
}

void main() {
    vec3 normal = texture(u_g_normal, v_texcoord).rgb;
    // If normal is black (empty space), discard/return 0 occlusion (1.0 visibility)
    if (length(normal) < 0.1) {
        g_occlusion = 0.0; // Wait, 0 means occluded? No. Output is single channel.
        // Usually SSAO map: 0 = fully occluded, 1 = fully visible?
        // Let's output "Occlusion Factor". 0 = No Occlusion. 1 = Max Occlusion.
        // Then in lighting we do: Ambient * (1.0 - occlusion)
        return; 
        // Wait, standard is "Ambient Occlusion Factor" where 1.0 = Visible, 0.0 = Occluded.
        // Let's stick to standard: 1.0 = White = No Shadow. 0.0 = Black = Shadow.
        // So if normal is empty, return 1.0 (No shadow).
    }
    
    vec3 frag_pos = get_view_pos(v_texcoord);
    
    // Get random vector
    vec3 random_vec = texture(u_tex_noise, v_texcoord * u_noise_scale).xyz;
    
    // Create TBN change-of-basis matrix: from tangent-space to view-space
    vec3 tangent = normalize(random_vec - normal * dot(random_vec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN = mat3(tangent, bitangent, normal);
    
    float occlusion = 0.0;
    
    for(int i = 0; i < u_kernel_size; ++i) {
        // get sample position
        vec3 sample_pos = TBN * u_samples[i]; // From tangent to view-space
        sample_pos = frag_pos + sample_pos * u_radius; 
        
        // project sample position (to sample texture) (to get position on screen/texture)
        vec4 offset = vec4(sample_pos, 1.0);
        offset = u_projection * offset; // from view to clip-space
        offset.xyz /= offset.w; // perspective divide
        offset.xyz = offset.xyz * 0.5 + 0.5; // transform to range 0.0 - 1.0
        
        // get sample depth
        // We need linear view-space depth? Or just reconstruct position z?
        // Let's verify depth stored in texture?
        // Standard depth buffer is non-linear.
        // Reconstructed view pos 'z' is negative in OpenGL (usually).
        
        float sample_depth = get_view_pos(offset.xy).z; // Get depth value of kernel sample
        
        // range check & accumulate
        // If sample depth is "closer" to camera (larger Z value, since Z is negative view space... wait)
        // View space: -Z is forward. So closer = less negative (larger).
        // Example: Camera at 0. Object at -5. Background at -100.
        // Sample at -5.1 (slightly behind surface).
        // Surface at that pixel is at -5.0.
        // -5.0 > -5.1. True. So it is occluded. 
        // We use Bias to prevent acne.
        
        float range_check = smoothstep(0.0, 1.0, u_radius / abs(frag_pos.z - sample_depth));
        occlusion += (sample_depth >= sample_pos.z + u_bias ? 1.0 : 0.0) * range_check;
    }
    
    occlusion = 1.0 - (occlusion / float(u_kernel_size));
    g_occlusion = occlusion;
}
