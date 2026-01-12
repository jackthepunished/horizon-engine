#version 410 core

/**
 * Deferred Geometry Pass - Vertex Shader
 * Outputs to G-Buffer for deferred lighting
 */

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;
layout(location = 3) in vec3 a_Tangent;

// Camera UBO
layout(std140) uniform CameraData {
    mat4 u_View;
    mat4 u_Projection;
    mat4 u_ViewProjection;
    vec4 u_ViewPos;
    vec4 u_ViewportSize;
};

uniform mat4 u_Model;
uniform mat4 u_NormalMatrix;

// For TAA jittering
uniform vec2 u_JitterOffset;

// Motion vectors
uniform mat4 u_PrevViewProjection;
uniform mat4 u_PrevModel;

out VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoord;
    vec3 Tangent;
    vec4 ClipPos;
    vec4 PrevClipPos;
} vs_out;

void main() {
    vec4 worldPos = u_Model * vec4(a_Position, 1.0);
    vs_out.FragPos = worldPos.xyz;
    vs_out.Normal = mat3(u_NormalMatrix) * a_Normal;
    vs_out.TexCoord = a_TexCoord;
    vs_out.Tangent = mat3(u_NormalMatrix) * a_Tangent;
    
    // Current clip position (with jitter for TAA)
    vec4 clipPos = u_ViewProjection * worldPos;
    clipPos.xy += u_JitterOffset * clipPos.w;
    vs_out.ClipPos = clipPos;
    
    // Previous frame clip position (for motion vectors)
    vec4 prevWorldPos = u_PrevModel * vec4(a_Position, 1.0);
    vs_out.PrevClipPos = u_PrevViewProjection * prevWorldPos;
    
    gl_Position = clipPos;
}
