#pragma once

/**
 * @file model.hpp
 * @brief Model loader with skeletal animation support
 */

#include "asset_handle.hpp"
#include "engine/animation/skeleton.hpp"
#include "engine/assets/texture.hpp"
#include "engine/core/types.hpp"
#include "engine/renderer/mesh.hpp"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace hz {

/**
 * @brief Material data loaded from FBX file
 */
struct FBXMaterial {
    std::string name;

    // PBR textures (loaded and owned by Model)
    std::shared_ptr<Texture> albedo_texture;
    std::shared_ptr<Texture> normal_texture;
    std::shared_ptr<Texture> metallic_roughness_texture;
    std::shared_ptr<Texture> ao_texture;
    std::shared_ptr<Texture> emissive_texture;

    // Fallback colors if no texture
    glm::vec3 albedo_color{1.0f};
    float metallic{0.0f};
    float roughness{0.5f};
    glm::vec3 emissive_color{0.0f};
};

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
     * @brief Load model from FBX file
     */
    [[nodiscard]] static Model load_from_fbx(std::string_view path);

    /**
     * @brief Draw all meshes
     */
    void draw() const;

    /**
     * @brief Setup instancing for all meshes
     */
    void setup_instancing(const std::vector<glm::mat4>& instance_transforms);

    /**
     * @brief Draw all meshes instanced
     */
    void draw_instanced(u32 instance_count) const;

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

    /**
     * @brief Get FBX materials (for rendering)
     */
    [[nodiscard]] const std::vector<FBXMaterial>& fbx_materials() const { return m_fbx_materials; }

    /**
     * @brief Check if model has FBX materials
     */
    [[nodiscard]] bool has_fbx_materials() const noexcept { return !m_fbx_materials.empty(); }

private:
    std::vector<Mesh> m_meshes;
    std::string m_path;

    // Skeletal animation data
    std::shared_ptr<Skeleton> m_skeleton;
    std::vector<std::shared_ptr<AnimationClip>> m_animations;

    // FBX-specific material data
    std::vector<FBXMaterial> m_fbx_materials;
};

} // namespace hz
