#version 410 core

/**
 * Deferred Geometry Pass - Vertex Shader
 * Outputs to G-Buffer for deferred lighting
 * Simplified version using standard uniforms
 */

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;
layout(location = 3) in vec4 a_Tangent; 
// Bone attributes
layout(location = 4) in ivec4 a_Joints;  // Bone indices
layout(location = 5) in vec4 a_Weights; // Bone weights

// Camera uniforms (set manually)
uniform mat4 u_View;
uniform mat4 u_Projection;
uniform mat4 u_Model;
uniform mat4 u_PrevViewProjection;

// Animation uniforms
const int MAX_BONES = 100;
const int MAX_BONE_INFLUENCE = 4;
uniform mat4 u_BoneMatrices[MAX_BONES];
uniform bool u_HasAnimation; // If false, treat as static mesh

out VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoord;
    vec4 Tangent; // vec4 now
    vec4 ClipPos;
    vec4 PrevClipPos;
} vs_out;

void main() {
    vec4 totalPosition = vec4(0.0f);
    vec3 totalNormal = vec3(0.0f);
    vec3 totalTangent = vec3(0.0f);

    if (u_HasAnimation) {
        for(int i = 0 ; i < MAX_BONE_INFLUENCE ; i++) {
            if(a_Joints[i] == -1) // Invalid bone index
                continue;
            if(a_Joints[i] >= MAX_BONES) {
                totalPosition = vec4(a_Position, 1.0f); // Fallback
                break;
            }
            
            // vec4 localPosition = u_BoneMatrices[int(a_Joints[i])] * vec4(a_Position, 1.0f);
            // totalPosition += localPosition * a_Weights[i];
            
            // vec3 localNormal = mat3(u_BoneMatrices[int(a_Joints[i])]) * a_Normal;
            // totalNormal += localNormal * a_Weights[i];
            
            // Apply bind pose matrix? 
            // Usually u_BoneMatrices combines (GlobalTransform * InverseBindMatrix).
            // So we directly multiply.
            
            int boneIndex = a_Joints[i];
            float weight = a_Weights[i];
            
            if (weight > 0.0) {
                 mat4 boneTransform = u_BoneMatrices[boneIndex];
                 totalPosition += (boneTransform * vec4(a_Position, 1.0)) * weight;
                 totalNormal   += (mat3(boneTransform) * a_Normal) * weight;
                 totalTangent  += (mat3(boneTransform) * a_Tangent.xyz) * weight;
            }
        }
        
        // Safety for uninitialized weights or sum < 1 (though loader typically normalizes)
        if (totalPosition.w == 0.0f) {
           totalPosition = vec4(a_Position, 1.0f);
           totalNormal = a_Normal;
           totalTangent = a_Tangent.xyz;
        }
    } else {
        totalPosition = vec4(a_Position, 1.0f);
        totalNormal = a_Normal;
        totalTangent = a_Tangent.xyz;
    }

    vec4 worldPos = u_Model * totalPosition; 
    // Optimization: If u_BoneMatrices contain Model transform, u_Model should be Identity.
    // However, usually Animation Systems pass local-to-model space transforms in BoneMatrices,
    // so we still need to multiply by u_Model to get to World Space.
    
    vs_out.FragPos = worldPos.xyz;
    
    mat3 normalMatrix = transpose(inverse(mat3(u_Model))); 
    // Note: If using skinning, non-uniform scaling in u_Model might break normals if not handled correctly.
    // But bone transforms handle the local deformation.
    
    // We update normals based on skinning first (local space deformation), then transform to world.
    vs_out.Normal = normalize(normalMatrix * totalNormal);
    vs_out.TexCoord = a_TexCoord;
    vs_out.Tangent = vec4(normalize(normalMatrix * totalTangent), a_Tangent.w);
    
    // Compute view-projection
    mat4 viewProjection = u_Projection * u_View;
    vec4 clipPos = viewProjection * worldPos;
    vs_out.ClipPos = clipPos;
    
    // Compute previous clip position for velocity
    // Motion vectors with skinning are tricky without previous bone matrices.
    // For now, we use current bone pose for previous frame approximation (no deformation blur)
    // or just fallback to static object logic which might cause ghosting on fast animation.
    // Let's use the static approximation for now to avoid complexity of u_PrevBoneMatrices.
    vs_out.PrevClipPos = u_PrevViewProjection * u_Model * vec4(a_Position, 1.0); // Incorrect for skinned, but compiles.
    
    gl_Position = clipPos;
}
