#include "model.hpp"

#include "engine/core/log.hpp"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

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
