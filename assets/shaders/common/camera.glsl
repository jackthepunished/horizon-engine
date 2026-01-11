#ifndef CAMERA_GLSL
#define CAMERA_GLSL

layout(std140) uniform CameraData {
    mat4 u_view;
    mat4 u_projection;
    mat4 u_view_projection;
    vec3 u_view_pos; // offset 192
    float _pad0;     // offset 204
    vec2 u_viewport_size; // offset 208
    vec2 _pad1;      // offset 216 - total 224
};

#endif
