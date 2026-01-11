#ifndef FOG_GLSL
#define FOG_GLSL

// Uniforms provided by SceneData UBO
// int u_fog_enabled;
// float u_fog_density;
// vec4 u_fog_color;

vec3 apply_fog(vec3 color, float dist) {
    if (u_fog_enabled != 0) { // Check int as bool
        float fog_factor = 1.0 - exp(-u_fog_density * dist);
        fog_factor = clamp(fog_factor, 0.0, 1.0);
        return mix(color, u_fog_color.rgb, fog_factor);
    }
    return color;
}

#endif
