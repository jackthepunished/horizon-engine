#pragma once

/**
 * @file model.hpp
 * @brief Model loader with skeletal animation support
 */

#include "asset_handle.hpp"
#include "engine/animation/skeleton.hpp"
#include "engine/core/types.hpp"
#include "engine/renderer/mesh.hpp"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace hz {

/**
 * @brief Loaded 3D model with meshes, skeleton, and animations
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
     * @brief Load model from GLTF file (with optional skeleton/animations)
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
     * @brief Check if model has skeleton
     */
    [[nodiscard]] bool has_skeleton() const noexcept { return m_skeleton != nullptr; }

    /**
     * @brief Get skeleton (may be null)
     */
    [[nodiscard]] std::shared_ptr<Skeleton> skeleton() const { return m_skeleton; }

    /**
     * @brief Get animations
     */
    [[nodiscard]] const std::vector<std::shared_ptr<AnimationClip>>& animations() const {
        return m_animations;
    }

    /**
     * @brief Get animation by name
     */
    [[nodiscard]] std::shared_ptr<AnimationClip> get_animation(const std::string& name) const {
        for (const auto& anim : m_animations) {
            if (anim->name == name)
                return anim;
        }
        return nullptr;
    }

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

    // Skeletal animation data
    std::shared_ptr<Skeleton> m_skeleton;
    std::vector<std::shared_ptr<AnimationClip>> m_animations;
};

} // namespace hz
