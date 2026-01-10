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

            HZ_ENGINE_INFO("GLTF Primitive: {} vertices", pos_accessor.count);

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

                // Debug first vertex
                if (i == 0) {
                    HZ_ENGINE_INFO("  First Vertex: Pos({:.2f}, {:.2f}, {:.2f})", v.position.x,
                                   v.position.y, v.position.z);
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
                HZ_ENGINE_INFO("  Indices: {}", indices.size());
            }

            if (!vertices.empty()) {
                model.m_meshes.emplace_back(std::move(vertices), std::move(indices));
            }
        }
    }

    HZ_ENGINE_INFO("Loaded GLTF Model: {} ({} meshes)", path, model.m_meshes.size());
    return model;
}

} // namespace hz
