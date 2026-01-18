#version 410 core

/**
 * Depth-only shadow map vertex shader (directional light)
 */

layout(location = 0) in vec3 a_Position;

// Bone attributes
layout(location = 4) in ivec4 a_Joints;  // Bone indices
layout(location = 5) in vec4 a_Weights; // Bone weights

uniform mat4 u_Model;
uniform mat4 u_LightSpaceMatrix;

// Animation uniforms
const int MAX_BONES = 100;
const int MAX_BONE_INFLUENCE = 4;
uniform mat4 u_BoneMatrices[MAX_BONES];
uniform bool u_HasAnimation;

void main() {
    vec4 totalPosition = vec4(0.0f);

    if (u_HasAnimation) {
        for(int i = 0 ; i < MAX_BONE_INFLUENCE ; i++) {
            if(a_Joints[i] == -1) 
                continue;
            
            int boneIndex = a_Joints[i];
            float weight = a_Weights[i];
            
            if (boneIndex >= MAX_BONES) break;

            if (weight > 0.0) {
                 mat4 boneTransform = u_BoneMatrices[boneIndex];
                 totalPosition += (boneTransform * vec4(a_Position, 1.0)) * weight;
            }
        }
        if (totalPosition.w == 0.0f) totalPosition = vec4(a_Position, 1.0f);
    } else {
        totalPosition = vec4(a_Position, 1.0f);
    }

    gl_Position = u_LightSpaceMatrix * u_Model * totalPosition; 
}

