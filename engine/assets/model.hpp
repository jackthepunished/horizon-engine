#pragma once

/**
 * @file model.hpp
 * @brief OBJ model loader
 */

#include "asset_handle.hpp"
#include "engine/core/types.hpp"
#include "engine/renderer/mesh.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace hz {

/**
 * @brief Loaded 3D model with multiple meshes
 */
class Model {
public:
    Model() = default;
    ~Model() = default;

    HZ_NON_COPYABLE(Model);
    HZ_DEFAULT_MOVABLE(Model);

    /**
     * @brief Load model from OBJ file
     */
    [[nodiscard]] static Model load_from_obj(std::string_view path);

    /**
     * @brief Load model from GLTF file
     */
    [[nodiscard]] static Model load_from_gltf(std::string_view path);

    /**
     * @brief Draw all meshes
     */
    void draw() const;

    /**
     * @brief Check if model is valid
     */
    [[nodiscard]] bool is_valid() const noexcept { return !m_meshes.empty(); }

    /**
     * @brief Get mesh count
     */
    [[nodiscard]] size_t mesh_count() const noexcept { return m_meshes.size(); }

    /**
     * @brief Get file path
     */
    [[nodiscard]] const std::string& path() const noexcept { return m_path; }

private:
    std::vector<Mesh> m_meshes;
    std::string m_path;
};

} // namespace hz
