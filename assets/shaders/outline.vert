#version 410 core

layout (location = 0) in vec3 a_position;
layout (location = 1) in vec3 a_normal;

uniform mat4 u_model;
uniform mat4 u_view_projection;
uniform float u_outline_thickness;

void main() {
    // Extrude vertex along normal for outline effect
    vec3 extruded_pos = a_position + a_normal * u_outline_thickness;
    gl_Position = u_view_projection * u_model * vec4(extruded_pos, 1.0);
}
