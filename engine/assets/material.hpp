#pragma once

/**
 * @file material.hpp
 * @brief PBR Material definition with texture handles
 */

#include "engine/assets/asset_handle.hpp"

#include <string>

#include <glm/glm.hpp>

namespace hz {

/**
 * @brief PBR Material with texture handles and fallback values
 *
 * Following LearnOpenGL/Hell2025 patterns - materials are first-class objects
 * that encapsulate all surface properties for rendering.
 */
struct Material {
    std::string name{"Default"};

    // ==========================================================================
    // PBR Properties (fallback values when textures are not present)
    // ==========================================================================
    glm::vec3 albedo_color{1.0f, 1.0f, 1.0f};
    float metallic{0.0f};
    float roughness{0.5f};
    float ao{1.0f};

    // UV tiling factor
    float uv_scale{1.0f};

    // ==========================================================================
    // Texture Handles (invalid handle = use fallback value)
    // ==========================================================================
    TextureHandle albedo_tex{};
    TextureHandle normal_tex{};
    TextureHandle metallic_tex{};
    TextureHandle roughness_tex{};
    TextureHandle ao_tex{};

    // ==========================================================================
    // Usage Flags (auto-derived from texture validity, but can be overridden)
    // ==========================================================================
    [[nodiscard]] bool has_albedo_tex() const { return albedo_tex.is_valid(); }
    [[nodiscard]] bool has_normal_tex() const { return normal_tex.is_valid(); }
    [[nodiscard]] bool has_metallic_tex() const { return metallic_tex.is_valid(); }
    [[nodiscard]] bool has_roughness_tex() const { return roughness_tex.is_valid(); }
    [[nodiscard]] bool has_ao_tex() const { return ao_tex.is_valid(); }

    // ==========================================================================
    // Factory Methods
    // ==========================================================================

    /**
     * @brief Create a simple solid-color material
     */
    static Material solid_color(const glm::vec3& color, float metallic = 0.0f,
                                float roughness = 0.5f) {
        Material mat;
        mat.name = "SolidColor";
        mat.albedo_color = color;
        mat.metallic = metallic;
        mat.roughness = roughness;
        return mat;
    }

    /**
     * @brief Create a default material (white, non-metallic, medium roughness)
     */
    static Material default_material() {
        Material mat;
        mat.name = "Default";
        return mat;
    }
};

// Forward declare for handle
class MaterialStorage;

/**
 * @brief Handle to a Material in the AssetRegistry
 */
using MaterialHandle = AssetHandle<Material>;

} // namespace hz
