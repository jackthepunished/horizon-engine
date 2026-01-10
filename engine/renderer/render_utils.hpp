#pragma once

/**
 * @file render_utils.hpp
 * @brief Utility functions for applying materials and rendering entities
 *
 * These helper functions reduce boilerplate in the main render loop by
 * centralizing material application and mesh drawing logic.
 */

#include "engine/assets/asset_registry.hpp"
#include "engine/assets/material.hpp"
#include "engine/renderer/mesh.hpp"
#include "engine/renderer/opengl/shader.hpp"
#include "engine/scene/components.hpp"

namespace hz {

/**
 * @brief Apply a material's properties to a PBR shader
 *
 * Sets all relevant uniforms (albedo, metallic, roughness, etc.) and binds
 * textures to the appropriate slots.
 *
 * @param shader The PBR shader to configure
 * @param material The material properties to apply
 * @param registry Asset registry for texture lookup
 */
inline void apply_material(gl::Shader& shader, const Material& material, AssetRegistry& registry) {
    // Set PBR uniforms
    shader.set_vec3("u_albedo", material.albedo_color);
    shader.set_float("u_metallic", material.metallic);
    shader.set_float("u_roughness", material.roughness);
    shader.set_float("u_ao", material.ao);
    shader.set_float("u_uv_scale", material.uv_scale);

    // Bind textures and set usage flags
    if (material.has_albedo_tex()) {
        if (auto* tex = registry.get_texture(material.albedo_tex)) {
            tex->bind(0);
            shader.set_bool("u_use_textures", true);
        }
    } else {
        shader.set_bool("u_use_textures", false);
    }

    if (material.has_normal_tex()) {
        if (auto* tex = registry.get_texture(material.normal_tex)) {
            tex->bind(1);
            shader.set_bool("u_use_normal_map", true);
        }
    } else {
        shader.set_bool("u_use_normal_map", false);
    }

    if (material.has_metallic_tex()) {
        if (auto* tex = registry.get_texture(material.metallic_tex)) {
            tex->bind(2);
            shader.set_bool("u_use_metallic_map", true);
        }
    } else {
        shader.set_bool("u_use_metallic_map", false);
    }

    if (material.has_roughness_tex()) {
        if (auto* tex = registry.get_texture(material.roughness_tex)) {
            tex->bind(3);
            shader.set_bool("u_use_roughness_map", true);
        }
    } else {
        shader.set_bool("u_use_roughness_map", false);
    }

    if (material.has_ao_tex()) {
        if (auto* tex = registry.get_texture(material.ao_tex)) {
            tex->bind(4);
            shader.set_bool("u_use_ao_map", true);
        }
    } else {
        shader.set_bool("u_use_ao_map", false);
    }
}

/**
 * @brief Apply material from MeshComponent (supports both new and legacy formats)
 *
 * Uses MaterialHandle if valid, otherwise falls back to legacy inline properties.
 */
inline void apply_material_from_component(gl::Shader& shader, const MeshComponent& mc,
                                          AssetRegistry& registry) {
    if (mc.material.is_valid()) {
        if (const Material* mat = registry.get_material(mc.material)) {
            apply_material(shader, *mat, registry);
            return;
        }
    }

    // Fallback: use legacy inline properties
    Material legacy_mat;
    legacy_mat.albedo_color = mc.albedo_color;
    legacy_mat.metallic = mc.metallic;
    legacy_mat.roughness = mc.roughness;
    legacy_mat.ao = 1.0f;
    // Note: Legacy texture paths would need to be loaded and converted to handles
    // For now, just use the inline values
    apply_material(shader, legacy_mat, registry);
}

/**
 * @brief Draw the appropriate mesh for a MeshComponent
 *
 * Handles both primitives (cube, plane, sphere) and loaded models.
 *
 * @param mc The mesh component
 * @param cube Reference to pre-created cube mesh
 * @param plane Reference to pre-created plane mesh
 * @param sphere Reference to pre-created sphere mesh (optional)
 * @param registry Asset registry for model lookup
 * @return true if something was drawn, false otherwise
 */
inline bool draw_mesh_component(const MeshComponent& mc, Mesh& cube, Mesh& plane, Mesh* sphere,
                                AssetRegistry& registry) {
    if (mc.mesh_type == MeshComponent::MeshType::Model && mc.model.is_valid()) {
        if (auto* model = registry.get_model(mc.model)) {
            model->draw();
            return true;
        }
    }

    // Use primitive_name (new) or mesh_path (legacy) for primitives
    const std::string& name =
        (mc.mesh_type == MeshComponent::MeshType::Primitive) ? mc.primitive_name : mc.mesh_path;

    if (name == "cube") {
        cube.draw();
        return true;
    }
    if (name == "plane") {
        plane.draw();
        return true;
    }
    if (name == "sphere" && sphere) {
        sphere->draw();
        return true;
    }

    // Legacy: check mesh_path for model paths
    if (mc.mesh_path.find(".gltf") != std::string::npos ||
        mc.mesh_path.find(".obj") != std::string::npos) {
        // For legacy GLTF paths, we'd need to load via registry
        // This is a migration path - new code should use ModelHandle
        return false;
    }

    return false;
}

} // namespace hz
