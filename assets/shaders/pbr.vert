#version 410 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_texcoord;
layout(location = 3) in vec3 a_tangent;

layout(location = 4) in vec4 a_bone_ids;
layout(location = 5) in vec4 a_weights;

layout(location = 6) in mat4 a_instance_matrix; // Occupies 6, 7, 8, 9

uniform mat4 u_model;
uniform mat4 u_view_projection;
uniform mat4 u_light_space_matrix;
uniform bool u_instanced;

const int MAX_BONES = 100;
const int MAX_BONE_INFLUENCE = 4;
uniform mat4 u_bone_matrices[MAX_BONES];
uniform bool u_has_animations;

out vec3 v_world_pos;
out vec2 v_texcoord;
out vec4 v_frag_pos_light_space;
out mat3 v_TBN;

void main() {
    vec4 total_pos = vec4(0.0);
    vec3 total_normal = vec3(0.0);
    vec3 total_tangent = vec3(0.0);

    // Apply skinning if active
    if (u_has_animations) {
        for(int i = 0 ; i < MAX_BONE_INFLUENCE ; i++) {
            int bone_id = int(a_bone_ids[i]);
            if(bone_id == -1) 
                continue;
            
            if(bone_id >= MAX_BONES) {
                total_pos = vec4(a_position, 1.0);
                break;
            }
            
            vec4 local_pos = u_bone_matrices[bone_id] * vec4(a_position, 1.0);
            total_pos += local_pos * a_weights[i];
            
            mat3 normal_mat = mat3(u_bone_matrices[bone_id]);
            total_normal += (normal_mat * a_normal) * a_weights[i];
            total_tangent += (normal_mat * a_tangent) * a_weights[i];
        }
        
        // Safety check if weights sum to 0
        if (length(total_normal) < 0.001) {
             total_pos = vec4(a_position, 1.0);
             total_normal = a_normal;
             total_tangent = a_tangent;
        }
    } else {
        total_pos = vec4(a_position, 1.0);
        total_normal = a_normal;
        total_tangent = a_tangent;
    }

    mat4 model_matrix = u_instanced ? a_instance_matrix : u_model;
    vec4 world_pos = model_matrix * total_pos;
    v_world_pos = vec3(world_pos);
    v_texcoord = a_texcoord;
    
    // Compute TBN matrix for normal mapping
    mat3 normal_matrix = mat3(transpose(inverse(model_matrix)));
    vec3 T = normalize(normal_matrix * total_tangent);
    vec3 N = normalize(normal_matrix * total_normal);
    T = normalize(T - dot(T, N) * N); // Gram-Schmidt
    vec3 B = cross(N, T);
    v_TBN = mat3(T, B, N);
    
    v_frag_pos_light_space = u_light_space_matrix * world_pos;
    gl_Position = u_view_projection * world_pos;
}
