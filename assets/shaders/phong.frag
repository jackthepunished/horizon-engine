#version 410 core

out vec4 frag_color;

in vec3 v_frag_pos;
in vec2 v_texcoord;
in vec4 v_frag_pos_light_space;
in mat3 v_TBN;

// Material
uniform sampler2D u_diffuse_map;
uniform sampler2D u_shadow_map;
uniform sampler2D u_normal_map;
uniform bool u_use_normal_map;
uniform vec3 u_specular_color;  // Usually vec3(1.0) or material specific
uniform float u_shininess;      // e.g. 32.0

// Camera
uniform vec3 u_view_pos;

// Lighting Definitions matching Renderer
struct DirectionalLight {
    vec3 direction; // Direction FROM light
    vec3 color;
    float intensity;
};

struct PointLight {
    vec3 position;
    vec3 color;
    float intensity;
    float range;
};

struct SpotLight {
    vec3 position;
    vec3 direction;
    vec3 color;
    float intensity;
    float range;
    float cut_off;
    float outer_cut_off;
};

// Uniforms
uniform DirectionalLight u_sun;
uniform vec3 u_ambient_light;

#define MAX_POINT_LIGHTS 16
uniform int u_point_light_count;
uniform PointLight u_point_lights[MAX_POINT_LIGHTS];

#define MAX_SPOT_LIGHTS 16
uniform int u_spot_light_count;
uniform SpotLight u_spot_lights[MAX_SPOT_LIGHTS];

float calculate_shadow(vec4 frag_pos_light_space, vec3 normal, vec3 light_dir) {
    // perform perspective divide
    vec3 proj_coords = frag_pos_light_space.xyz / frag_pos_light_space.w;
    // transform to [0,1] range
    proj_coords = proj_coords * 0.5 + 0.5;
    
    // keep the shadow at 0.0 when outside the far_plane region of the light's frustum.
    if(proj_coords.z > 1.0)
        return 0.0;
        
    // get closest depth value from light's perspective (using [0,1] range fragPosLight as coords)
    float closest_depth = texture(u_shadow_map, proj_coords.xy).r; 
    // get depth of current fragment from light's perspective
    float current_depth = proj_coords.z;
    
    // check whether current frag pos is in shadow
    // PCF (Percentage-closer filtering)
    float shadow = 0.0;
    // bias to prevent shadow acne
    float bias = max(0.005 * (1.0 - dot(normal, light_dir)), 0.0005);
    
    vec2 texel_size = 1.0 / textureSize(u_shadow_map, 0);
    for(int x = -1; x <= 1; ++x)
    {
        for(int y = -1; y <= 1; ++y)
        {
            float pcf_depth = texture(u_shadow_map, proj_coords.xy + vec2(x, y) * texel_size).r; 
            shadow += current_depth - bias > pcf_depth ? 1.0 : 0.0;        
        }    
    }
    shadow /= 9.0;
    
    return shadow;
}

// Functions
vec3 calculate_directional_light(DirectionalLight light, vec3 normal, vec3 view_dir, vec3 material_diffuse, vec3 material_specular, float shadow) {
    vec3 light_dir = normalize(-light.direction);
    
    // Diffuse
    float diff = max(dot(normal, light_dir), 0.0);
    
    // Specular
    vec3 reflect_dir = reflect(-light_dir, normal);
    float spec = pow(max(dot(view_dir, reflect_dir), 0.0), u_shininess);
    
    vec3 diffuse = light.color * light.intensity * diff * material_diffuse;
    vec3 specular = light.color * light.intensity * spec * material_specular;
    
    return (1.0 - shadow) * (diffuse + specular);
}

vec3 calculate_point_light(PointLight light, vec3 normal, vec3 frag_pos, vec3 view_dir, vec3 material_diffuse, vec3 material_specular) {
    vec3 light_dir = normalize(light.position - frag_pos);
    
    // Diffuse
    float diff = max(dot(normal, light_dir), 0.0);
    
    // Specular
    vec3 reflect_dir = reflect(-light_dir, normal);
    float spec = pow(max(dot(view_dir, reflect_dir), 0.0), u_shininess);
    
    // Attenuation
    float distance = length(light.position - frag_pos);
    // Simple linear/quadratic attenuation or just range based
    // Using a simple falloff: 1.0 / (1.0 + 0.09*d + 0.032*d*d) is common
    // For now, let's use a linear falloff based on range for simplicity and control
    float attenuation = clamp(1.0 - (distance / light.range), 0.0, 1.0);
    attenuation *= attenuation; // Quadratic-ish
    
    vec3 diffuse = light.color * light.intensity * diff * material_diffuse;
    vec3 specular = light.color * light.intensity * spec * material_specular;
    
    return (diffuse + specular) * attenuation;
}

void main() {
    // Texture Color
    vec4 tex_color = texture(u_diffuse_map, v_texcoord);
    if(tex_color.a < 0.1) discard;
    vec3 material_diffuse = tex_color.rgb;
    vec3 material_specular = u_specular_color; // Basic assumption
    
    // Normal - either from normal map or vertex normal
    vec3 norm;
    if (u_use_normal_map) {
        // Sample normal map (stored in tangent space, [0,1] -> [-1,1])
        norm = texture(u_normal_map, v_texcoord).rgb;
        norm = norm * 2.0 - 1.0; // Transform to [-1, 1]
        norm = normalize(v_TBN * norm); // Transform to world space
    } else {
        // Use geometric normal (third column of TBN)
        norm = normalize(v_TBN[2]);
    }
    
    vec3 view_dir = normalize(u_view_pos - v_frag_pos);
    
    vec3 result = u_ambient_light * material_diffuse;
    
    // Sun
    vec3 sun_dir_norm = normalize(-u_sun.direction); // Direction TO light
    float shadow = calculate_shadow(v_frag_pos_light_space, norm, sun_dir_norm);
    result += calculate_directional_light(u_sun, norm, view_dir, material_diffuse, material_specular, shadow);
    
    // Point Lights
    for(int i = 0; i < u_point_light_count; ++i) {
        result += calculate_point_light(u_point_lights[i], norm, v_frag_pos, view_dir, material_diffuse, material_specular);
    }
    
    // Spot Lights (TODO)
    
    // Gamma correction
    // result = pow(result, vec3(1.0/2.2)); 
    
    frag_color = vec4(result, tex_color.a);
}
