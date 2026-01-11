#ifndef LIGHTS_GLSL
#define LIGHTS_GLSL

// std140 layout compatible structs (must match C++ side)
// vec3 has 16-byte alignment but 12-byte size in std140, which causes
// offset mismatches. Using vec4 ensures proper alignment.

struct DirectionalLight {
    vec4 direction; // xyz = direction, w = padding
    vec4 color;     // xyz = color, w = padding
    vec4 intensity; // x = intensity, yzw = padding
}; // Total: 48 bytes

#define MAX_POINT_LIGHTS 16
struct PointLight {
    vec4 position;  // xyz = position, w = padding
    vec4 color;     // xyz = color, w = padding
    float intensity;
    float range;
    float _pad[2];  // explicit padding to 48 bytes
}; // Total: 48 bytes

// Uniforms are now provided via SceneData UBO in common/scene_data.glsl

#endif
