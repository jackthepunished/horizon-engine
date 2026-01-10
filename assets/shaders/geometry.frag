#version 410 core

layout(location = 0) out vec3 g_normal;

in vec3 v_view_normal;

void main() {
    // Store View-Space Normals in RGB buffer
    g_normal = normalize(v_view_normal);
}
