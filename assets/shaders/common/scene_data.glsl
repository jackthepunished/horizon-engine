#ifndef SCENE_DATA_GLSL
#define SCENE_DATA_GLSL

#include "lights.glsl"

layout(std140) uniform SceneData {
    DirectionalLight u_sun;
    vec4 u_ambient_light; // .w unused/padding
    float u_time;
    int u_fog_enabled;
    float u_fog_density;
    float u_fog_gradient;
    vec4 u_fog_color;
    
    // Fixed size array for point lights
    int u_point_light_count;
    // Padding to align array?
    // PointLight is 48 bytes (vec3+float * 3). 
    // Array stride is 48.
    // Base alignment of PointLight struct is 16.
    // So Array starts at offset divisible by 16.
    
    // However, PointLight array might be large.
    // For now, let's include it. MAX_POINT_LIGHTS = 16.
    // 16 * 48 = 768 bytes. Small enough.
    PointLight u_point_lights[16]; 
};

#endif
