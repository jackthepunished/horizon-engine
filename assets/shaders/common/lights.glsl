#ifndef LIGHTS_GLSL
#define LIGHTS_GLSL

struct DirectionalLight {
    vec3 direction;
    vec3 color;
    float intensity;
};

#define MAX_POINT_LIGHTS 16
struct PointLight {
    vec3 position;
    vec3 color;
    float intensity;
    float range;
};

// Uniforms expected by shaders using lights
// Note: Uniforms cannot be in a block easily without UBOs, 
// so we assume standard naming conventions here.
uniform DirectionalLight u_sun;
uniform vec3 u_ambient_light;
uniform int u_point_light_count;
uniform PointLight u_point_lights[MAX_POINT_LIGHTS];

#endif
