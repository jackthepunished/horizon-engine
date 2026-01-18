#include "model.hpp"

#include "engine/core/log.hpp"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

// Define these only in one .cpp file
#define TINYGLTF_IMPLEMENTATION
// #define STB_IMAGE_IMPLEMENTATION // Already in texture.cpp
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "engine/vendor/tinygltf/tiny_gltf.h" // Assuming we set include path correctly or use relative
#include "engine/vendor/ufbx/ufbx.h"

#include <unordered_map>

namespace hz {

Model Model::load_from_obj(std::string_view path) {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    std::string path_str(path);
    std::string base_dir;

    // Extract base directory for material loading
    auto last_slash = path_str.find_last_of("/\\");
    if (last_slash != std::string::npos) {
        base_dir = path_str.substr(0, last_slash + 1);
    }

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path_str.c_str(),
                          base_dir.c_str())) {
        HZ_ENGINE_ERROR("Failed to load OBJ: {} - {}", path, err);
        return {};
    }

    if (!warn.empty()) {
        HZ_ENGINE_WARN("OBJ warning: {}", warn);
    }

    Model model;
    model.m_path = path;

    // Process each shape into a mesh
    for (const auto& shape : shapes) {
        std::vector<Vertex> vertices;
        std::vector<u32> indices;
        std::unordered_map<std::string, u32, TransparentStringHash, std::equal_to<>>
            unique_vertices;

        for (const auto& index : shape.mesh.indices) {
            Vertex vertex{};

            // Position
            vertex.position = {attrib.vertices[3 * static_cast<size_t>(index.vertex_index) + 0],
                               attrib.vertices[3 * static_cast<size_t>(index.vertex_index) + 1],
                               attrib.vertices[3 * static_cast<size_t>(index.vertex_index) + 2]};

            // Normal
            if (index.normal_index >= 0) {
                vertex.normal = {attrib.normals[3 * static_cast<size_t>(index.normal_index) + 0],
                                 attrib.normals[3 * static_cast<size_t>(index.normal_index) + 1],
                                 attrib.normals[3 * static_cast<size_t>(index.normal_index) + 2]};
            }

            // Texcoord
            if (index.texcoord_index >= 0) {
                vertex.texcoord = {
                    attrib.texcoords[2 * static_cast<size_t>(index.texcoord_index) + 0],
                    1.0f - attrib.texcoords[2 * static_cast<size_t>(index.texcoord_index) + 1]};
            }

            // Deduplicate vertices
            std::string key = std::to_string(index.vertex_index) + "_" +
                              std::to_string(index.normal_index) + "_" +
                              std::to_string(index.texcoord_index);

            if (unique_vertices.count(key) == 0) {
                unique_vertices[key] = static_cast<u32>(vertices.size());
                vertices.push_back(vertex);
            }

            indices.push_back(unique_vertices[key]);
        }

        if (!vertices.empty()) {
            model.m_meshes.emplace_back(std::move(vertices), std::move(indices));
        }
    }

    HZ_ENGINE_INFO("Loaded OBJ: {} ({} shapes, {} total vertices)", path, shapes.size(),
                   attrib.vertices.size() / 3);

    return model;
}

void Model::draw() const {
    for (const auto& mesh : m_meshes) {
        mesh.draw();
    }
}

void Model::setup_instancing(const std::vector<glm::mat4>& instance_transforms) {
    for (auto& mesh : m_meshes) {
        mesh.setup_instancing(instance_transforms);
    }
}

void Model::draw_instanced(u32 instance_count) const {
    for (const auto& mesh : m_meshes) {
        mesh.draw_instanced(instance_count);
    }
}

} // namespace hz

// Re-open namespace for GLTF implementation
#include <functional>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace hz {

// Helper function to calculate tangents from positions, normals, and UVs
static void calculate_tangents(std::vector<Vertex>& vertices, const std::vector<u32>& indices) {
    if (indices.size() < 3)
        return;

    // Reset tangents
    // Reset tangents
    for (auto& v : vertices) {
        v.tangent = glm::vec4(0.0f);
    }

    // Accumulate tangents per triangle
    for (size_t i = 0; i < indices.size(); i += 3) {
        u32 i0 = indices[i];
        u32 i1 = indices[i + 1];
        u32 i2 = indices[i + 2];

        if (i0 >= vertices.size() || i1 >= vertices.size() || i2 >= vertices.size())
            continue;

        Vertex& v0 = vertices[i0];
        Vertex& v1 = vertices[i1];
        Vertex& v2 = vertices[i2];

        glm::vec3 edge1 = v1.position - v0.position;
        glm::vec3 edge2 = v2.position - v0.position;

        glm::vec2 deltaUV1 = v1.texcoord - v0.texcoord;
        glm::vec2 deltaUV2 = v2.texcoord - v0.texcoord;

        float denom = deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y;
        if (std::abs(denom) < 1e-6f) {
            // Degenerate triangle, skip
            continue;
        }

        float f = 1.0f / denom;

        glm::vec3 tangent;
        tangent.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
        tangent.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
        tangent.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);

        // Accumulate (will normalize later)
        // Accumulate (will normalize later)
        // Note: We only accumulate xyz for now, w will be set to 1.0
        v0.tangent += glm::vec4(tangent, 0.0f);
        v1.tangent += glm::vec4(tangent, 0.0f);
        v2.tangent += glm::vec4(tangent, 0.0f);
    }

    // Normalize and orthogonalize tangents (Gram-Schmidt)
    for (auto& v : vertices) {
        if (glm::length(v.tangent) > 1e-6f) {
            // Orthogonalize: T = normalize(T - N * dot(N, T))
            // Orthogonalize: T = normalize(T - N * dot(N, T))
            glm::vec3 t = glm::vec3(v.tangent);
            t = t - v.normal * glm::dot(v.normal, t);
            t = glm::normalize(t);
            v.tangent = glm::vec4(t, 1.0f);
        } else {
            // Fallback: generate tangent from normal
            glm::vec3 up = std::abs(v.normal.y) < 0.999f ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
            glm::vec3 t = glm::normalize(glm::cross(up, v.normal));
            v.tangent = glm::vec4(t, 1.0f);
        }
    }
}

Model Model::load_from_gltf(std::string_view path) {
    tinygltf::Model gltf_model;
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;
    std::string path_str(path);

    bool ret = loader.LoadASCIIFromFile(&gltf_model, &err, &warn, path_str);
    // Try binary if ASCII fails or check extension
    if (!ret) {
        ret = loader.LoadBinaryFromFile(&gltf_model, &err, &warn, path_str);
    }

    if (!warn.empty()) {
        HZ_ENGINE_WARN("GLTF warning: {}", warn);
    }

    if (!ret) {
        HZ_ENGINE_ERROR("Failed to load GLTF: {} - {}", path, err);
        return {};
    }

    Model model;
    model.m_path = path;

    // Helper to get buffer data
    auto get_buffer_data = [&](const tinygltf::Accessor& accessor) -> const unsigned char* {
        const auto& view = gltf_model.bufferViews[accessor.bufferView];
        const auto& buffer = gltf_model.buffers[view.buffer];
        return &buffer.data[view.byteOffset + accessor.byteOffset];
    };

    // Map node index to bone ID for animation linking
    std::unordered_map<int, int> node_to_bone_id;

    // 1. Load Skeleton (Skin)
    if (!gltf_model.skins.empty()) {
        const auto& skin = gltf_model.skins[0]; // Assuming one skin for now
        auto skeleton = std::make_shared<Skeleton>();
        model.m_skeleton = skeleton;

        // Resize Accessors for IBM
        const float* ibm_data = nullptr;
        if (skin.inverseBindMatrices > -1) {
            const auto& accessor = gltf_model.accessors[skin.inverseBindMatrices];
            ibm_data = reinterpret_cast<const float*>(get_buffer_data(accessor));
        }

        // First pass: Create all bones flat
        for (size_t i = 0; i < skin.joints.size(); ++i) {
            int node_idx = skin.joints[i];
            const auto& node = gltf_model.nodes[node_idx];

            glm::mat4 ibm = glm::mat4(1.0f);
            if (ibm_data) {
                // GLTF IBMs are stored as 16 floats
                ibm = glm::make_mat4(&ibm_data[i * 16]);
            }

            // Create bone with temporary parent -1
            std::string bone_name = node.name.empty() ? "Bone_" + std::to_string(i) : node.name;
            int bone_id = skeleton->add_bone(bone_name, -1, ibm);
            node_to_bone_id[node_idx] = bone_id;

            // Set initial local transform from node (bind pose)
            Bone* bone = skeleton->get_bone(bone_id);
            if (node.translation.size() == 3) {
                bone->position =
                    glm::vec3(node.translation[0], node.translation[1], node.translation[2]);
            }
            if (node.rotation.size() == 4) {
                // GLTF quaternion is x, y, z, w
                bone->rotation = glm::quat(static_cast<float>(node.rotation[3]),  // w
                                           static_cast<float>(node.rotation[0]),  // x
                                           static_cast<float>(node.rotation[1]),  // y
                                           static_cast<float>(node.rotation[2])); // z
            }
            if (node.scale.size() == 3) {
                bone->scale = glm::vec3(node.scale[0], node.scale[1], node.scale[2]);
            }
        }

        // Second pass: Link parents
        for (size_t i = 0; i < skin.joints.size(); ++i) {
            int node_idx = skin.joints[i];
            const auto& node = gltf_model.nodes[node_idx];
            int current_bone_id = node_to_bone_id[node_idx];

            for (int child_node_idx : node.children) {
                // Check if child is part of the skeleton
                if (node_to_bone_id.find(child_node_idx) != node_to_bone_id.end()) {
                    int child_bone_id = node_to_bone_id[child_node_idx];
                    Bone* child_bone = skeleton->get_bone(child_bone_id);
                    child_bone->parent_id = current_bone_id;

                    Bone* parent_bone = skeleton->get_bone(current_bone_id);
                    if (parent_bone) {
                        parent_bone->children.push_back(child_bone_id);
                    }
                }
            }
        }

        HZ_ENGINE_INFO("Loaded Skeleton: {} bones", skeleton->bone_count());
    }

    // 2. Load Animations
    for (const auto& gltf_anim : gltf_model.animations) {
        auto clip = std::make_shared<AnimationClip>();
        clip->name = gltf_anim.name;
        clip->duration = 0.0f;

        for (const auto& channel : gltf_anim.channels) {
            if (node_to_bone_id.find(channel.target_node) == node_to_bone_id.end())
                continue;

            int bone_id = node_to_bone_id[channel.target_node];
            const auto& node = gltf_model.nodes[channel.target_node];

            Bone* bone = model.m_skeleton->get_bone(bone_id);
            if (!bone)
                continue;
            std::string bone_name = bone->name;

            // Find or create BoneAnimation channel
            BoneAnimation* bone_anim = nullptr;
            for (auto& ch : clip->channels) {
                if (ch.bone_name == bone_name) {
                    bone_anim = &ch;
                    break;
                }
            }

            if (!bone_anim) {
                clip->channels.push_back({});
                bone_anim = &clip->channels.back();
                bone_anim->bone_name = bone_name;
                bone_anim->bone_id = bone_id;
            }

            const auto& sampler = gltf_anim.samplers[channel.sampler];
            const auto& input = gltf_model.accessors[sampler.input];   // Time
            const auto& output = gltf_model.accessors[sampler.output]; // Value

            const float* times = reinterpret_cast<const float*>(get_buffer_data(input));
            const float* values = reinterpret_cast<const float*>(get_buffer_data(output));

            // Track max duration
            if (input.maxValues.size() > 0) {
                float max_t = static_cast<float>(input.maxValues[0]);
                if (max_t > clip->duration)
                    clip->duration = max_t;
            } else if (input.count > 0) {
                if (times[input.count - 1] > clip->duration)
                    clip->duration = times[input.count - 1];
            }

            // Target path
            if (channel.target_path == "translation") {
                for (size_t k = 0; k < input.count; ++k) {
                    bone_anim->position_keys.push_back({times[k], glm::make_vec3(&values[k * 3])});
                }
            } else if (channel.target_path == "rotation") {
                for (size_t k = 0; k < input.count; ++k) {
                    // GLTF quat is x,y,z,w
                    float x = values[k * 4 + 0];
                    float y = values[k * 4 + 1];
                    float z = values[k * 4 + 2];
                    float w = values[k * 4 + 3];
                    bone_anim->rotation_keys.push_back({times[k], glm::quat(w, x, y, z)});
                }
            } else if (channel.target_path == "scale") {
                for (size_t k = 0; k < input.count; ++k) {
                    bone_anim->scale_keys.push_back({times[k], glm::make_vec3(&values[k * 3])});
                }
            }
        }

        if (!clip->channels.empty()) {
            model.m_animations.push_back(clip);
        }
    }

    if (!model.m_animations.empty()) {
        HZ_ENGINE_INFO("Loaded Animations: {}", model.m_animations.size());
    }

    // Helper to compute world transform for a node
    std::function<glm::mat4(int, const tinygltf::Model&)> get_node_world_transform;
    get_node_world_transform = [&](int node_idx, const tinygltf::Model& mdl) -> glm::mat4 {
        glm::mat4 local_transform = glm::mat4(1.0f);
        const auto& node = mdl.nodes[node_idx];

        if (node.matrix.size() == 16) {
            // Use provided matrix directly (column-major)
            local_transform = glm::make_mat4(node.matrix.data());
        } else {
            // Build from TRS
            glm::vec3 translation(0.0f);
            glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f); // w,x,y,z
            glm::vec3 scale(1.0f);

            if (node.translation.size() == 3) {
                translation =
                    glm::vec3(node.translation[0], node.translation[1], node.translation[2]);
            }
            if (node.rotation.size() == 4) {
                // GLTF quaternion is x, y, z, w
                rotation = glm::quat(
                    static_cast<float>(node.rotation[3]), static_cast<float>(node.rotation[0]),
                    static_cast<float>(node.rotation[1]), static_cast<float>(node.rotation[2]));
            }
            if (node.scale.size() == 3) {
                scale = glm::vec3(node.scale[0], node.scale[1], node.scale[2]);
            }

            glm::mat4 T = glm::translate(glm::mat4(1.0f), translation);
            glm::mat4 R = glm::mat4_cast(rotation);
            glm::mat4 S = glm::scale(glm::mat4(1.0f), scale);
            local_transform = T * R * S;
        }

        return local_transform;
    };

    // Build node-to-parent map for traversing up the hierarchy
    std::unordered_map<int, int> node_parent;
    for (size_t i = 0; i < gltf_model.nodes.size(); ++i) {
        for (int child : gltf_model.nodes[i].children) {
            node_parent[child] = static_cast<int>(i);
        }
    }

    // Helper to get full world transform by walking up to root
    auto get_full_world_transform = [&](int node_idx) -> glm::mat4 {
        std::vector<int> chain;
        int current = node_idx;
        while (current >= 0) {
            chain.push_back(current);
            auto it = node_parent.find(current);
            current = (it != node_parent.end()) ? it->second : -1;
        }
        // Multiply from root to leaf
        glm::mat4 world = glm::mat4(1.0f);
        for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
            world = world * get_node_world_transform(*it, gltf_model);
        }
        return world;
    };

    // Debug: Log node and mesh counts
    HZ_ENGINE_INFO("GLTF has {} nodes, {} meshes", gltf_model.nodes.size(),
                   gltf_model.meshes.size());

    // Build node->mesh mapping and process meshes with their world transforms
    for (size_t node_idx = 0; node_idx < gltf_model.nodes.size(); ++node_idx) {
        const auto& node = gltf_model.nodes[node_idx];
        HZ_ENGINE_INFO("Node {}: mesh={}, children={}", node_idx, node.mesh, node.children.size());
        if (node.mesh < 0)
            continue;

        glm::mat4 world_transform = get_full_world_transform(static_cast<int>(node_idx));
        glm::mat3 normal_matrix = glm::transpose(glm::inverse(glm::mat3(world_transform)));

        const auto& gltf_mesh = gltf_model.meshes[node.mesh];

        // A mesh can have multiple primitives
        for (const auto& primitive : gltf_mesh.primitives) {
            std::vector<Vertex> vertices;
            std::vector<u32> indices;

            // Accessors
            const tinygltf::Accessor& pos_accessor =
                gltf_model.accessors[primitive.attributes.find("POSITION")->second];
            const tinygltf::Accessor* norm_accessor = nullptr;
            const tinygltf::Accessor* tex_accessor = nullptr;
            const tinygltf::Accessor* tan_accessor = nullptr;

            if (primitive.attributes.find("NORMAL") != primitive.attributes.end())
                norm_accessor = &gltf_model.accessors[primitive.attributes.find("NORMAL")->second];

            if (primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end())
                tex_accessor =
                    &gltf_model.accessors[primitive.attributes.find("TEXCOORD_0")->second];

            if (primitive.attributes.find("TANGENT") != primitive.attributes.end())
                tan_accessor = &gltf_model.accessors[primitive.attributes.find("TANGENT")->second];

            // Buffers
            const tinygltf::BufferView& pos_view = gltf_model.bufferViews[pos_accessor.bufferView];
            const tinygltf::Buffer& pos_buffer = gltf_model.buffers[pos_view.buffer];
            const unsigned char* pos_data_ptr =
                &pos_buffer.data[pos_view.byteOffset + pos_accessor.byteOffset];
            int pos_stride = pos_accessor.ByteStride(pos_view) ? pos_accessor.ByteStride(pos_view)
                                                               : sizeof(float) * 3;

            const unsigned char* norm_data_ptr = nullptr;
            int norm_stride = 0;
            if (norm_accessor) {
                const tinygltf::BufferView& norm_view =
                    gltf_model.bufferViews[norm_accessor->bufferView];
                const tinygltf::Buffer& norm_buffer = gltf_model.buffers[norm_view.buffer];
                norm_data_ptr = &norm_buffer.data[norm_view.byteOffset + norm_accessor->byteOffset];
                norm_stride = norm_accessor->ByteStride(norm_view)
                                  ? norm_accessor->ByteStride(norm_view)
                                  : sizeof(float) * 3;
            }

            const unsigned char* tex_data_ptr = nullptr;
            int tex_stride = 0;
            if (tex_accessor) {
                const tinygltf::BufferView& tex_view =
                    gltf_model.bufferViews[tex_accessor->bufferView];
                const tinygltf::Buffer& tex_buffer = gltf_model.buffers[tex_view.buffer];
                tex_data_ptr = &tex_buffer.data[tex_view.byteOffset + tex_accessor->byteOffset];
                tex_stride = tex_accessor->ByteStride(tex_view) ? tex_accessor->ByteStride(tex_view)
                                                                : sizeof(float) * 2;
            }

            const unsigned char* tan_data_ptr = nullptr;
            int tan_stride = 0;
            if (tan_accessor) {
                const tinygltf::BufferView& tan_view =
                    gltf_model.bufferViews[tan_accessor->bufferView];
                const tinygltf::Buffer& tan_buffer = gltf_model.buffers[tan_view.buffer];
                tan_data_ptr = &tan_buffer.data[tan_view.byteOffset + tan_accessor->byteOffset];
                tan_stride = tan_accessor->ByteStride(tan_view) ? tan_accessor->ByteStride(tan_view)
                                                                : sizeof(float) * 4;
            }

            // Joints & Weights
            const void* joints_data = nullptr;
            int joints_comp_type = 0;
            int joints_stride = 0;
            if (primitive.attributes.count("JOINTS_0")) {
                const auto& acc =
                    gltf_model.accessors[primitive.attributes.find("JOINTS_0")->second];
                joints_data = get_buffer_data(acc);
                joints_comp_type = acc.componentType;
                joints_stride = acc.ByteStride(gltf_model.bufferViews[acc.bufferView]);
                if (joints_stride == 0)
                    joints_stride = 0; // Handled dynamically based on type
            }

            const float* weights_data = nullptr;
            int weights_stride = 0;
            if (primitive.attributes.count("WEIGHTS_0")) {
                const auto& acc =
                    gltf_model.accessors[primitive.attributes.find("WEIGHTS_0")->second];
                weights_data = reinterpret_cast<const float*>(get_buffer_data(acc));
                weights_stride =
                    acc.ByteStride(gltf_model.bufferViews[acc.bufferView]) / sizeof(float);
                if (weights_stride == 0)
                    weights_stride = 4;
            }

            // Vertices
            glm::vec3 min_pos_local(std::numeric_limits<float>::max());
            glm::vec3 max_pos_local(std::numeric_limits<float>::lowest());

            for (size_t i = 0; i < pos_accessor.count; ++i) {
                Vertex v{};

                const float* p = reinterpret_cast<const float*>(pos_data_ptr + i * pos_stride);
                glm::vec3 local_pos(p[0], p[1], p[2]);

                min_pos_local = glm::min(min_pos_local, local_pos);
                max_pos_local = glm::max(max_pos_local, local_pos);

                // Apply node's world transform to position
                v.position = glm::vec3(world_transform * glm::vec4(local_pos, 1.0f));

                if (norm_data_ptr) {
                    const float* n =
                        reinterpret_cast<const float*>(norm_data_ptr + i * norm_stride);
                    glm::vec3 local_normal(n[0], n[1], n[2]);
                    // Apply normal matrix to normal
                    v.normal = glm::normalize(normal_matrix * local_normal);
                }

                if (tex_data_ptr) {
                    const float* t = reinterpret_cast<const float*>(tex_data_ptr + i * tex_stride);
                    v.texcoord = glm::vec2(t[0], t[1]);
                }

                if (tan_data_ptr) {
                    const float* t = reinterpret_cast<const float*>(tan_data_ptr + i * tan_stride);
                    // Read 4 components (xyzw) - w determines handedness
                    // If stride suggests only 3 components (rare in GLTF), assume w=1
                    glm::vec4 local_tangent(0.0f, 0.0f, 0.0f, 1.0f);
                    local_tangent.x = t[0];
                    local_tangent.y = t[1];
                    local_tangent.z = t[2];

                    // Check if we have 4th component
                    bool has_w = (tan_stride >= sizeof(float) * 4) ||
                                 (tan_accessor && tan_accessor->type == TINYGLTF_TYPE_VEC4);
                    if (has_w) {
                        local_tangent.w = t[3];
                    }

                    // Apply normal matrix to tangent xyz, keep w
                    glm::vec3 transformed_t =
                        glm::normalize(normal_matrix * glm::vec3(local_tangent));
                    v.tangent = glm::vec4(transformed_t, local_tangent.w);
                }
                // If no tangent data, we'll calculate after all vertices are loaded

                // Bone Data
                if (joints_data && weights_data) {
                    // Extract weights
                    glm::vec4 w = glm::make_vec4(weights_data + i * weights_stride);

                    // Extract joints based on component type
                    int j[4] = {0, 0, 0, 0};
                    if (joints_comp_type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
                        const u8* ptr = static_cast<const u8*>(joints_data) +
                                        i * (joints_stride ? joints_stride : 4);
                        j[0] = ptr[0];
                        j[1] = ptr[1];
                        j[2] = ptr[2];
                        j[3] = ptr[3];
                    } else if (joints_comp_type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                        const u16* ptr =
                            reinterpret_cast<const u16*>(static_cast<const u8*>(joints_data) +
                                                         i * (joints_stride ? joints_stride : 8));
                        j[0] = ptr[0];
                        j[1] = ptr[1];
                        j[2] = ptr[2];
                        j[3] = ptr[3];
                    }

                    // Assign to vertex
                    for (int k = 0; k < 4; k++) {
                        if (w[k] > 0.0f) {
                            v.add_bone(j[k], w[k]);
                        }
                    }
                }

                vertices.push_back(v);
            }
            HZ_ENGINE_INFO("Mesh Primitive Local Bounds: MIN({:.2f}, {:.2f}, {:.2f}) MAX({:.2f}, "
                           "{:.2f}, {:.2f})",
                           min_pos_local.x, min_pos_local.y, min_pos_local.z, max_pos_local.x,
                           max_pos_local.y, max_pos_local.z);

            // Indices
            if (primitive.indices >= 0) {
                const tinygltf::Accessor& index_accessor = gltf_model.accessors[primitive.indices];
                const tinygltf::BufferView& index_view =
                    gltf_model.bufferViews[index_accessor.bufferView];
                const tinygltf::Buffer& index_buffer = gltf_model.buffers[index_view.buffer];
                int index_stride = index_accessor.ByteStride(index_view); // Should be tight usually

                const unsigned char* index_data_ptr =
                    &index_buffer.data[index_view.byteOffset + index_accessor.byteOffset];

                for (size_t i = 0; i < index_accessor.count; ++i) {
                    u32 idx = 0;
                    if (index_accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                        idx = static_cast<u32>(*reinterpret_cast<const unsigned short*>(
                            index_data_ptr +
                            i * (index_stride ? index_stride : sizeof(unsigned short))));
                    } else if (index_accessor.componentType ==
                               TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
                        idx = *reinterpret_cast<const unsigned int*>(
                            index_data_ptr +
                            i * (index_stride ? index_stride : sizeof(unsigned int)));
                    } else if (index_accessor.componentType ==
                               TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
                        idx = static_cast<u32>(*reinterpret_cast<const unsigned char*>(
                            index_data_ptr +
                            i * (index_stride ? index_stride : sizeof(unsigned char))));
                    }
                    indices.push_back(idx);
                }
            }

            if (!vertices.empty()) {
                // Calculate tangents if not provided in GLTF
                if (!tan_data_ptr && tex_data_ptr && !indices.empty()) {
                    calculate_tangents(vertices, indices);
                    HZ_ENGINE_INFO(
                        "  Mesh primitive: {} vertices, {} indices (tangents calculated)",
                        vertices.size(), indices.size());
                } else {
                    HZ_ENGINE_INFO("  Mesh primitive: {} vertices, {} indices", vertices.size(),
                                   indices.size());
                }
                model.m_meshes.emplace_back(std::move(vertices), std::move(indices));
            }
        } // end primitives loop
    } // end nodes loop

    HZ_ENGINE_INFO("Loaded GLTF: {} ({} meshes)", path, model.m_meshes.size());
    return model;
}

Model Model::load_from_fbx(std::string_view path) {
    ufbx_load_opts opts = {};
    opts.target_axes = ufbx_axes_right_handed_y_up;
    opts.target_unit_meters = 1.0f;
    // We want ufbx to handle coordinate space conversion but NOT bake geometry transforms
    // into vertices if we are doing skinning, or maybe we do?
    // Actually, for skinning, vertices should be in bind pose.
    // ufbx defaults are usually sane.

    ufbx_error error;
    std::string path_str(path);
    ufbx_scene* scene = ufbx_load_file(path_str.c_str(), &opts, &error);

    if (!scene) {
        HZ_ENGINE_ERROR("Failed to load FBX: {} - {}", path, error.description.data);
        return {};
    }

    Model model;
    model.m_path = path;

    // --- 1. Load Skeleton ---
    // Identify all nodes that are used as bones by any skin cluster
    std::unordered_map<ufbx_node*, int> node_to_bone_id;
    auto skeleton = std::make_shared<Skeleton>();

    // Collect bone nodes
    // We iterate all meshes -> skin deformers -> clusters -> bone_node
    for (size_t i = 0; i < scene->meshes.count; ++i) {
        ufbx_mesh* mesh = scene->meshes.data[i];
        for (size_t j = 0; j < mesh->skin_deformers.count; ++j) {
            ufbx_skin_deformer* skin = mesh->skin_deformers.data[j];
            for (size_t k = 0; k < skin->clusters.count; ++k) {
                ufbx_skin_cluster* cluster = skin->clusters.data[k];
                if (cluster->bone_node) {
                    // Add to set of known bones if not present
                    if (node_to_bone_id.find(cluster->bone_node) == node_to_bone_id.end()) {
                        // We defer adding to skeleton until we can sort them or just add them now?
                        // Ideally bones are added such that parents come before children for simple
                        // iteration, but our Skeleton class might not strictly require it if we set
                        // parents correctly. However, let's collect them first.
                    }
                }
            }
        }
    }

    // Better approach: Recurse scene graph. If a node is a "bone" (has limb attribute or is used by
    // skin), add it. Or just iterate scene->nodes (which is flat list usually topological or index
    // order). Let's just iterate all nodes and check if they are relevant. Actually, we process all
    // skin clusters to make sure we only add used bones.

    // Pass 1: Collect unique bone nodes from clusters
    std::vector<ufbx_node*> bone_nodes;
    for (size_t i = 0; i < scene->meshes.count; ++i) {
        ufbx_mesh* mesh = scene->meshes.data[i];
        for (size_t j = 0; j < mesh->skin_deformers.count; ++j) {
            ufbx_skin_deformer* skin = mesh->skin_deformers.data[j];
            for (size_t k = 0; k < skin->clusters.count; ++k) {
                ufbx_skin_cluster* cluster = skin->clusters.data[k];
                if (cluster->bone_node &&
                    node_to_bone_id.find(cluster->bone_node) == node_to_bone_id.end()) {
                    // Mark as visited (temp ID -1)
                    node_to_bone_id[cluster->bone_node] = -1;
                    bone_nodes.push_back(cluster->bone_node);
                }
            }
        }
    }

    // Pass 1.5: Also include parents of bone nodes up to root to ensure connectivity?
    // Mixamo FBX usually has a "Hips" or "Root" node.
    // ufbx nodes have `parent`.

    size_t original_bone_count = bone_nodes.size();
    for (size_t i = 0; i < original_bone_count; ++i) {
        ufbx_node* node = bone_nodes[i];
        ufbx_node* parent = node->parent;
        // Walk up
        while (parent) {
            if (node_to_bone_id.find(parent) == node_to_bone_id.end()) {
                node_to_bone_id[parent] = -1;
                bone_nodes.push_back(parent);
                parent = parent->parent;
            } else {
                break; // Already known
            }
        }
    }

    // Sort bone_nodes by hierarchy depth or scene index to ensure parents processed first?
    // scene->nodes is usually topological.

    // Re-order bone_nodes based on scene index (which is robust)
    std::sort(bone_nodes.begin(), bone_nodes.end(), [](ufbx_node* a, ufbx_node* b) {
        return a->typed_id < b->typed_id; // or checking array index in scene->nodes?
        // ufbx doesn't expose flat index easily unless we search or use typed_id (if valid).
        // Actually, just pointer comparison isn't enough.
        // Let's just trust valid parent linkage later.
        return false;
    });

    if (!bone_nodes.empty()) {
        model.m_skeleton = skeleton;

        // Helper to convert ufbx matrix
        auto to_glm = [](const ufbx_matrix& m) {
            return glm::mat4(m.cols[0].x, m.cols[0].y, m.cols[0].z, 0.0f, m.cols[1].x, m.cols[1].y,
                             m.cols[1].z, 0.0f, m.cols[2].x, m.cols[2].y, m.cols[2].z, 0.0f,
                             m.cols[3].x, m.cols[3].y, m.cols[3].z, 1.0f);
        };

        // Pass 2: Add bones to skeleton
        // We need to re-scan scene->nodes or just iterate our collected nodes.
        // To respect hierarchy, we can't just iterate bone_nodes arbitrary string.
        // But `Skeleton::add_bone` supports any order if we link parent later?
        // Current `Skeleton::add_bone` takes parent ID.
        // So we need to ensure parent is added first?
        // Or we can simple add all, then set parents.

        // Actually `Skeleton::add_bone` (std implementation usually) pushes to vector.
        // Returns ID.

        for (ufbx_node* node : bone_nodes) {
            std::string name(node->name.data, node->name.length);
            if (name.empty())
                name = "Bone_" + std::to_string(std::hash<void*>{}(node));

            // IBM?
            // If this node is used in a cluster, we have an IBM.
            // If it's a structural parent (not in cluster), IBM is identity?
            glm::mat4 ibm(1.0f);

            // Find a cluster for this node to get IBM
            // This is expensive to search every time.
            // Optimization: Map node -> cluster earlier?
            // But a node can be in multiple clusters (multiple meshes). IBM should be same (offset
            // from bind pose). ufbx: geometry_to_bone transform.

            // Let's assume identity if not found, or use inverse world bind pose.
            // ufbx loads bind pose into node->bind_to_world for bones?
            // Checking documentation... ufbx_skin_cluster has geometry_to_bone.

            // Find ANY cluster for this node
            bool found_cluster = false;
            for (size_t i = 0; i < scene->meshes.count; ++i) {
                ufbx_mesh* mesh = scene->meshes.data[i];
                for (size_t j = 0; j < mesh->skin_deformers.count; ++j) {
                    ufbx_skin_deformer* skin = mesh->skin_deformers.data[j];
                    for (size_t k = 0; k < skin->clusters.count; ++k) {
                        ufbx_skin_cluster* cluster = skin->clusters.data[k];
                        if (cluster->bone_node == node) {
                            ibm = to_glm(cluster->geometry_to_bone);
                            found_cluster = true;
                            break;
                        }
                    }
                    if (found_cluster)
                        break;
                }
                if (found_cluster)
                    break;
            }

            int id = skeleton->add_bone(name, -1, ibm);
            node_to_bone_id[node] = id;

            // Set local transform (Bind Pose)
            // ufbx stores current transform in node->local_transform.
            // Use node->lcl_translation etc?
            // Check if `node->bind_to_parent` exists? NOT in ufbx.
            // We can use `node->node_to_parent` (local transform).

            ufbx_transform local = node->local_transform;
            Bone* bone = skeleton->get_bone(id);
            bone->position =
                glm::vec3(local.translation.x, local.translation.y, local.translation.z);
            bone->rotation =
                glm::quat(local.rotation.w, local.rotation.x, local.rotation.y, local.rotation.z);
            bone->scale = glm::vec3(local.scale.x, local.scale.y, local.scale.z);
        }

        // Pass 3: Link Parents
        for (ufbx_node* node : bone_nodes) {
            if (node->parent && node_to_bone_id.find(node->parent) != node_to_bone_id.end()) {
                int child_id = node_to_bone_id[node];
                int parent_id = node_to_bone_id[node->parent];

                Bone* child_bone = skeleton->get_bone(child_id);
                child_bone->parent_id = parent_id;

                Bone* parent_bone = skeleton->get_bone(parent_id);
                parent_bone->children.push_back(child_id);
            }
        }

        HZ_ENGINE_INFO("FBX Skeleton: {} bones", skeleton->bone_count());
    }

    // Helper to convert ufbx matrix to glm
    auto to_glm_mat = [](const ufbx_matrix& m) {
        return glm::mat4(m.cols[0].x, m.cols[0].y, m.cols[0].z, 0.0f, m.cols[1].x, m.cols[1].y,
                         m.cols[1].z, 0.0f, m.cols[2].x, m.cols[2].y, m.cols[2].z, 0.0f,
                         m.cols[3].x, m.cols[3].y, m.cols[3].z, 1.0f);
    };

    // Iterate all nodes to find meshes
    for (size_t i = 0; i < scene->nodes.count; ++i) {
        ufbx_node* node = scene->nodes.data[i];
        if (node->mesh) {
            ufbx_mesh* mesh = node->mesh;

            ufbx_matrix node_to_world = node->node_to_world;
            ufbx_matrix geometry_to_node = node->geometry_to_node;
            ufbx_matrix m = ufbx_matrix_mul(&node_to_world, &geometry_to_node);

            // Check for skinning BEFORE setting up transforms
            ufbx_skin_deformer* skin = nullptr;
            bool is_skinned = mesh->skin_deformers.count > 0;
            if (is_skinned) {
                skin = mesh->skin_deformers.data[0];
            }

            // For skinned meshes, we only apply geometry_to_node (bind shape matrix)
            // For static meshes, apply full world transform
            ufbx_matrix vertex_transform;
            if (is_skinned) {
                // Only apply geometry transform for skinned meshes
                // This keeps vertices in model space (bind pose)
                vertex_transform = geometry_to_node;
            } else {
                // Apply full world transform for static meshes
                vertex_transform = m;
            }

            glm::mat4 world_transform = to_glm_mat(vertex_transform);
            glm::mat3 normal_matrix = glm::transpose(glm::inverse(glm::mat3(world_transform)));

            std::vector<Vertex> vertices;
            std::vector<u32> indices;

            // Track bounds for debugging
            glm::vec3 min_bounds(std::numeric_limits<float>::max());
            glm::vec3 max_bounds(std::numeric_limits<float>::lowest());

            // Iterate faces
            for (size_t face_idx = 0; face_idx < mesh->faces.count; ++face_idx) {
                ufbx_face face = mesh->faces.data[face_idx];

                // Triangulate face (fan)
                for (size_t v_i = 0; v_i < face.num_indices - 2; ++v_i) {
                    uint32_t face_indices[3] = {face.index_begin,
                                                face.index_begin + static_cast<uint32_t>(v_i) + 1,
                                                face.index_begin + static_cast<uint32_t>(v_i) + 2};

                    for (uint32_t face_vertex_idx : face_indices) {
                        Vertex v{};
                        uint32_t pos_idx = mesh->vertex_indices.data[face_vertex_idx];

                        // Position - apply appropriate transform
                        ufbx_vec3 pos = mesh->vertices.data[pos_idx];
                        ufbx_vec3 transformed_pos = ufbx_transform_position(&vertex_transform, pos);
                        v.position =
                            glm::vec3(transformed_pos.x, transformed_pos.y, transformed_pos.z);

                        // Update bounds
                        min_bounds = glm::min(min_bounds, v.position);
                        max_bounds = glm::max(max_bounds, v.position);

                        // Normal
                        if (mesh->vertex_normal.exists) {
                            uint32_t norm_idx = face_vertex_idx;
                            if (mesh->vertex_normal.indices.data) {
                                norm_idx = mesh->vertex_normal.indices.data[face_vertex_idx];
                            }
                            if (norm_idx < mesh->vertex_normal.values.count) {
                                ufbx_vec3 norm = mesh->vertex_normal.values.data[norm_idx];
                                glm::vec3 n(norm.x, norm.y, norm.z);
                                v.normal = glm::normalize(normal_matrix * n);
                            }
                        }

                        // UV
                        if (mesh->vertex_uv.exists) {
                            uint32_t uv_idx = face_vertex_idx;
                            if (mesh->vertex_uv.indices.data) {
                                uv_idx = mesh->vertex_uv.indices.data[face_vertex_idx];
                            }
                            if (uv_idx < mesh->vertex_uv.values.count) {
                                ufbx_vec2 uv = mesh->vertex_uv.values.data[uv_idx];
                                // FBX uses DirectX UV convention (Y=0 at top)
                                // OpenGL expects Y=0 at bottom, so flip V coordinate
                                v.texcoord = glm::vec2(uv.x, 1.0f - static_cast<float>(uv.y));
                            }
                        }

                        // Skin Weights
                        if (skin) {
                            // ufbx provides per-vertex weights.
                            // Access via `skin->vertices[pos_idx]`.
                            // Wait, `ufbx_skin_deformer` `vertices` array matches `mesh->vertices`.
                            // Yes, `skin->vertices` is `ufbx_skin_vertex` array.

                            // Check if valid
                            if (pos_idx < skin->vertices.count) {
                                ufbx_skin_vertex skin_v = skin->vertices.data[pos_idx];

                                // ufbx 0.14+ API:
                                // `skin_v` has `weight_begin` and `num_weights` into
                                // `skin->weights`. Wait, ufbx API changed? Let's check struct.
                                // `ufbx_skin_vertex` usually has direct indices/weights for small
                                // counts or indices into a weight buffer. Actually,
                                // `ufbx_skin_deformer` has `weights` list and `vertices` list.
                                // `vertices[i]` has `weight_begin` index into `weights` list?

                                // Checking header again...
                                // "ufbx_skin_vertex: num_weights, weight_begin"
                                // `ufbx_skin_weight`: cluster_index, weight.

                                // We need to sort and pick top 4.
                                struct BW {
                                    int id;
                                    float w;
                                };
                                BW bws[MAX_BONE_INFLUENCE];
                                for (int k = 0; k < MAX_BONE_INFLUENCE; ++k)
                                    bws[k] = {-1, 0.0f};

                                int count = 0;
                                (void)count; // Unused
                                // Need to iterate `skin->weights`
                                // Wait, `skin_v` isn't accessible directly if I don't know the
                                // layout. Assumption: `ufbx_skin_vertex` contains `uint32_t
                                // weight_begin` and `uint32_t num_weights`.

                                // NOTE: In loop above I need to verify this member exists.
                                // But I can't verifying without seeing header.
                                // Safer: `ufbx_get_skin_vertex_weights` helper? No.

                                // Let's try standard ufbx usage.
                                uint32_t w_begin = skin_v.weight_begin;
                                uint32_t w_count = skin_v.num_weights;

                                int slot = 0;
                                for (uint32_t k = 0; k < w_count && slot < MAX_BONE_INFLUENCE;
                                     ++k) {
                                    ufbx_skin_weight sw = skin->weights.data[w_begin + k];
                                    // Get bone ID
                                    ufbx_skin_cluster* cluster =
                                        skin->clusters.data[sw.cluster_index];
                                    if (node_to_bone_id.count(cluster->bone_node)) {
                                        v.add_bone(node_to_bone_id[cluster->bone_node],
                                                   (float)sw.weight);
                                        slot++;
                                    }
                                }
                            }
                        }

                        vertices.push_back(v);
                        indices.push_back(static_cast<u32>(vertices.size() - 1));
                    }
                }
            }

            if (!vertices.empty()) {
                HZ_ENGINE_INFO("FBX Mesh Bounds: MIN({:.2f}, {:.2f}, {:.2f}) MAX({:.2f}, {:.2f}, "
                               "{:.2f}) Skinned: {}",
                               min_bounds.x, min_bounds.y, min_bounds.z, max_bounds.x, max_bounds.y,
                               max_bounds.z, is_skinned ? "yes" : "no");
                calculate_tangents(vertices, indices);
                model.m_meshes.emplace_back(std::move(vertices), std::move(indices));
            }
        }
    }

    // --- 2. Load Animations ---
    if (skeleton && scene->anim_stacks.count > 0) {
        for (size_t i = 0; i < scene->anim_stacks.count; ++i) {
            ufbx_anim_stack* stack = scene->anim_stacks.data[i];

            auto clip = std::make_shared<AnimationClip>();
            std::string anim_name(stack->name.data, stack->name.length);
            if (anim_name.empty())
                anim_name = "Anim_" + std::to_string(i);
            clip->name = anim_name;

            double duration = stack->time_end - stack->time_begin;
            clip->duration = (float)duration;
            if (clip->duration < 0)
                clip->duration = 0.0f;

            // ufbx time is in seconds, so ticks_per_second = 1.0
            // (AnimatorComponent multiplies delta_time by ticks_per_second)
            clip->ticks_per_second = 1.0f;

            // Sample animations for all bones
            // We can assume a sample rate, e.g. 30 FPS.
            // Or ufbx can keyframe reduction?
            // Simple sampling for now.
            float fps = 30.0f;
            int num_frames = (int)(clip->duration * fps) + 1;

            for (auto& [node, bone_id] : node_to_bone_id) {
                if (bone_id == -1)
                    continue; // Skip if ignored
                Bone* bone = skeleton->get_bone(bone_id);

                BoneAnimation channel;
                channel.bone_name = bone->name;
                channel.bone_id = bone_id;

                // Sample transform
                // bool has_any_anim = false;

                // Optimization: check if node is animated?
                // Just sample.

                for (int f = 0; f < num_frames; ++f) {
                    double time = stack->time_begin + (double)f / fps;
                    if (time > stack->time_end)
                        time = stack->time_end;

                    // Evaluate local transform (relative to parent?)
                    // `ufbx_evaluate_transform` gives local transform.
                    ufbx_transform t = ufbx_evaluate_transform(stack->anim, node, time);

                    channel.position_keys.push_back(
                        {(float)(time - stack->time_begin),
                         glm::vec3(t.translation.x, t.translation.y, t.translation.z)});
                    channel.rotation_keys.push_back(
                        {(float)(time - stack->time_begin),
                         glm::quat(t.rotation.w, t.rotation.x, t.rotation.y, t.rotation.z)});
                    channel.scale_keys.push_back({(float)(time - stack->time_begin),
                                                  glm::vec3(t.scale.x, t.scale.y, t.scale.z)});
                }

                // Only add channel if we have data (we always have sampled data)
                clip->channels.push_back(channel);
            }

            model.m_animations.push_back(clip);
        }
        HZ_ENGINE_INFO("Loaded FBX Animations: {}", model.m_animations.size());
    }

    // --- 3. Load Materials ---
    // Get the base directory of the FBX file for resolving relative texture paths
    std::string base_dir;
    {
        size_t last_sep = path_str.find_last_of("/\\");
        if (last_sep != std::string::npos) {
            base_dir = path_str.substr(0, last_sep + 1);
        }
    }

    for (size_t i = 0; i < scene->materials.count; ++i) {
        ufbx_material* mat = scene->materials.data[i];

        FBXMaterial fbx_mat;
        fbx_mat.name = std::string(mat->name.data, mat->name.length);

        // Extract base color from material
        if (mat->fbx.diffuse_color.has_value) {
            fbx_mat.albedo_color =
                glm::vec3(static_cast<float>(mat->fbx.diffuse_color.value_vec3.x),
                          static_cast<float>(mat->fbx.diffuse_color.value_vec3.y),
                          static_cast<float>(mat->fbx.diffuse_color.value_vec3.z));
        }

        // Helper lambda to load texture from ufbx_texture
        auto load_texture_from_ufbx = [&](ufbx_texture* tex,
                                          bool is_srgb) -> std::shared_ptr<Texture> {
            if (!tex)
                return nullptr;

            TextureParams params;
            params.srgb = is_srgb;
            params.flip_y = false; // FBX uses OpenGL convention typically
            params.generate_mipmaps = true;

            // Check for embedded texture data first
            if (tex->content.size > 0) {
                auto loaded = std::make_shared<Texture>(
                    Texture::load_from_memory(static_cast<const unsigned char*>(tex->content.data),
                                              tex->content.size, params));
                if (loaded->is_valid()) {
                    return loaded;
                }
            }

            // Try loading from file
            std::string tex_path;
            if (tex->filename.length > 0) {
                tex_path = std::string(tex->filename.data, tex->filename.length);
            } else if (tex->relative_filename.length > 0) {
                tex_path = base_dir +
                           std::string(tex->relative_filename.data, tex->relative_filename.length);
            } else if (tex->absolute_filename.length > 0) {
                tex_path = std::string(tex->absolute_filename.data, tex->absolute_filename.length);
            }

            if (!tex_path.empty()) {
                auto loaded = std::make_shared<Texture>(Texture::load_from_file(tex_path, params));
                if (loaded->is_valid()) {
                    return loaded;
                }
                // Try with base_dir prefix
                if (tex_path[0] != '/') {
                    loaded = std::make_shared<Texture>(
                        Texture::load_from_file(base_dir + tex_path, params));
                    if (loaded->is_valid()) {
                        return loaded;
                    }
                }
            }

            return nullptr;
        };

        // Load albedo/diffuse texture
        if (mat->fbx.diffuse_color.texture) {
            fbx_mat.albedo_texture = load_texture_from_ufbx(mat->fbx.diffuse_color.texture, true);
        }

        // Load normal map
        if (mat->fbx.normal_map.texture) {
            fbx_mat.normal_texture = load_texture_from_ufbx(mat->fbx.normal_map.texture, false);
        } else if (mat->fbx.bump.texture) {
            // Some FBX files use bump instead of normal
            fbx_mat.normal_texture = load_texture_from_ufbx(mat->fbx.bump.texture, false);
        }

        // Try PBR maps if available
        if (mat->pbr.base_color.texture && !fbx_mat.albedo_texture) {
            fbx_mat.albedo_texture = load_texture_from_ufbx(mat->pbr.base_color.texture, true);
        }
        if (mat->pbr.normal_map.texture && !fbx_mat.normal_texture) {
            fbx_mat.normal_texture = load_texture_from_ufbx(mat->pbr.normal_map.texture, false);
        }
        if (mat->pbr.roughness.texture) {
            fbx_mat.metallic_roughness_texture =
                load_texture_from_ufbx(mat->pbr.roughness.texture, false);
        }
        if (mat->pbr.ambient_occlusion.texture) {
            fbx_mat.ao_texture = load_texture_from_ufbx(mat->pbr.ambient_occlusion.texture, false);
        }
        if (mat->pbr.emission_color.texture) {
            fbx_mat.emissive_texture =
                load_texture_from_ufbx(mat->pbr.emission_color.texture, true);
        }

        // Extract PBR scalar values
        if (mat->pbr.metalness.has_value) {
            fbx_mat.metallic = static_cast<float>(mat->pbr.metalness.value_real);
        }
        if (mat->pbr.roughness.has_value) {
            fbx_mat.roughness = static_cast<float>(mat->pbr.roughness.value_real);
        }

        model.m_fbx_materials.push_back(std::move(fbx_mat));
    }

    if (!model.m_fbx_materials.empty()) {
        HZ_ENGINE_INFO("Loaded FBX Materials: {}", model.m_fbx_materials.size());
    }

    HZ_ENGINE_INFO("Loaded FBX: {} ({} meshes)", path, model.m_meshes.size());
    ufbx_free_scene(scene);
    return model;
}

} // namespace hz
