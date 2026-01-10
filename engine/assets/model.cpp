#include "model.hpp"

#include "engine/core/log.hpp"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

// Define these only in one .cpp file
#define TINYGLTF_IMPLEMENTATION
// #define STB_IMAGE_IMPLEMENTATION // Already in texture.cpp
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "engine/vendor/tinygltf/tiny_gltf.h" // Assuming we set include path correctly or use relative

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
        std::unordered_map<std::string, u32> unique_vertices;

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

} // namespace hz

// Re-open namespace for GLTF implementation
#include <glm/gtc/type_ptr.hpp>

namespace hz {

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

    // Iterate over meshes
    for (const auto& gltf_mesh : gltf_model.meshes) {
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
            for (size_t i = 0; i < pos_accessor.count; ++i) {
                Vertex v{};

                const float* p = reinterpret_cast<const float*>(pos_data_ptr + i * pos_stride);
                v.position = glm::vec3(p[0], p[1], p[2]);

                if (norm_data_ptr) {
                    const float* n =
                        reinterpret_cast<const float*>(norm_data_ptr + i * norm_stride);
                    v.normal = glm::vec3(n[0], n[1], n[2]);
                }

                if (tex_data_ptr) {
                    const float* t = reinterpret_cast<const float*>(tex_data_ptr + i * tex_stride);
                    v.texcoord = glm::vec2(t[0], t[1]);
                }

                if (tan_data_ptr) {
                    const float* t = reinterpret_cast<const float*>(tan_data_ptr + i * tan_stride);
                    v.tangent = glm::vec3(t[0], t[1], t[2]); // Ignore w (handedness) for now
                } else {
                    // Default tangent if missing to prevent black geometry
                    v.tangent = glm::vec3(1.0f, 0.0f, 0.0f);
                }

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
                model.m_meshes.emplace_back(std::move(vertices), std::move(indices));
            }
        }
    }

    HZ_ENGINE_INFO("Loaded GLTF: {} ({} meshes)", path, model.m_meshes.size());
    return model;
}

} // namespace hz
