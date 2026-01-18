#version 410 core

/**
 * =============================================================================
 * HORIZON ENGINE - Uber PBR Vertex Shader
 * =============================================================================
 * 
 * Features:
 * - Standard vertex transformation
 * - TAA jittering
 * - Motion vectors
 * - Skeletal animation (optional)
 * =============================================================================
 */

// Vertex attributes
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;
layout(location = 3) in vec3 a_Tangent;

#ifdef HZ_FEATURE_SKINNING
layout(location = 4) in ivec4 a_BoneIDs;
layout(location = 5) in vec4 a_BoneWeights;
#endif

// Transform uniforms
uniform mat4 u_Model;
uniform mat4 u_PrevModel;  // For motion vectors
uniform mat3 u_NormalMatrix;

// Camera UBO (manual definition for OpenGL 4.1 compatibility)
layout(std140) uniform CameraData {
    mat4 u_View;
    mat4 u_Projection;
    mat4 u_ViewProjection;
    mat4 u_InverseView;
    mat4 u_InverseProjection;
    mat4 u_InverseViewProjection;
    mat4 u_PrevViewProjection;
    vec4 u_CameraPosition;
    vec4 u_CameraDirection;
    vec4 u_ViewportSize;
    vec4 u_ClipPlanes;
    vec4 u_JitterOffset;
};

// Shadow UBO
layout(std140) uniform ShadowData {
    mat4 u_LightSpaceMatrix;
};

#ifdef HZ_FEATURE_SKINNING
layout(std140) uniform SkinningData {
    mat4 u_BoneMatrices[128];
};
#endif

// Outputs
out VS_OUT {
    vec3 WorldPos;
    vec3 Normal;
    vec2 TexCoord;
    vec3 Tangent;
    vec3 Bitangent;
    vec4 LightSpacePos;
    vec4 ClipPos;
    vec4 PrevClipPos;
    float ViewDepth;
} vs_out;

void main() {
    vec4 localPos = vec4(a_Position, 1.0);
    vec3 localNormal = a_Normal;
    vec3 localTangent = a_Tangent;
    
    #ifdef HZ_FEATURE_SKINNING
    // Apply bone transforms
    mat4 boneTransform = mat4(0.0);
    for (int i = 0; i < 4; i++) {
        if (a_BoneIDs[i] >= 0) {
            boneTransform += u_BoneMatrices[a_BoneIDs[i]] * a_BoneWeights[i];
        }
    }
    localPos = boneTransform * localPos;
    localNormal = mat3(boneTransform) * localNormal;
    localTangent = mat3(boneTransform) * localTangent;
    #endif
    
    // World space
    vec4 worldPos = u_Model * localPos;
    vs_out.WorldPos = worldPos.xyz;
    
    // Normal, tangent, bitangent
    vs_out.Normal = normalize(u_NormalMatrix * localNormal);
    vs_out.Tangent = normalize(u_NormalMatrix * localTangent);
    vs_out.Bitangent = cross(vs_out.Normal, vs_out.Tangent);
    
    // Texture coordinates
    vs_out.TexCoord = a_TexCoord;
    
    // Shadow space
    vs_out.LightSpacePos = u_LightSpaceMatrix * worldPos;
    
    // Clip position with TAA jitter
    vec4 clipPos = u_ViewProjection * worldPos;
    vs_out.ClipPos = clipPos;
    
    // Apply jitter for TAA
    vec4 jitteredClipPos = clipPos;
    jitteredClipPos.xy += u_JitterOffset.xy * clipPos.w;
    
    // Previous frame position (for motion vectors)
    vec4 prevWorldPos = u_PrevModel * localPos;
    vs_out.PrevClipPos = u_PrevViewProjection * prevWorldPos;
    
    // View depth for CSM cascade selection
    vec4 viewPos = u_View * worldPos;
    vs_out.ViewDepth = -viewPos.z;
    
    gl_Position = jitteredClipPos;
}
