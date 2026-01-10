#pragma once

#include "engine/assets/asset_handle.hpp"
#include "engine/core/types.hpp"

#include <string>

#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

namespace hz {

// ==========================================
// Tag Component (Properties)
// ==========================================
struct TagComponent {
    std::string tag{"Entity"};

    // Serialization
    friend void to_json(nlohmann::json& j, const TagComponent& c) {
        j = nlohmann::json{{"tag", c.tag}};
    }
    friend void from_json(const nlohmann::json& j, TagComponent& c) { j.at("tag").get_to(c.tag); }
};

// ==========================================
// Transform Component
// ==========================================
struct TransformComponent {
    glm::vec3 position{0.0f};
    glm::vec3 rotation{0.0f}; // Euler angles in degrees
    glm::vec3 scale{1.0f};

    [[nodiscard]] glm::mat4 get_transform() const;

    // Helper for glm::vec3 serialization
    static nlohmann::json vec3_to_json(const glm::vec3& v) { return {v.x, v.y, v.z}; }
    static glm::vec3 vec3_from_json(const nlohmann::json& j) { return {j[0], j[1], j[2]}; }

    friend void to_json(nlohmann::json& j, const TransformComponent& c) {
        j = nlohmann::json{{"position", vec3_to_json(c.position)},
                           {"rotation", vec3_to_json(c.rotation)},
                           {"scale", vec3_to_json(c.scale)}};
    }
    friend void from_json(const nlohmann::json& j, TransformComponent& c) {
        c.position = vec3_from_json(j.at("position"));
        c.rotation = vec3_from_json(j.at("rotation"));
        c.scale = vec3_from_json(j.at("scale"));
    }
};

// ==========================================
// Mesh Component
// ==========================================
struct MeshComponent {
    // ==========================================================================
    // Mesh Source (new handle-based system)
    // ==========================================================================
    enum class MeshType : u8 {
        Primitive, // Built-in primitives: "cube", "sphere", "plane"
        Model      // Loaded model via ModelHandle
    };

    MeshType mesh_type{MeshType::Primitive};

    // For primitives (when mesh_type == Primitive)
    std::string primitive_name{"cube"}; // "cube", "sphere", "plane"

    // For loaded models (when mesh_type == Model)
    ModelHandle model{};

    // ==========================================================================
    // Material (new handle-based system - preferred)
    // ==========================================================================
    MaterialHandle material{};

    // ==========================================================================
    // Legacy Compatibility (for existing scenes without handles)
    // These are used as fallback if MaterialHandle is invalid
    // ==========================================================================
    std::string mesh_path{"cube"}; // Legacy: same as primitive_name for backward compat

    // Legacy material texture paths (deprecated - use MaterialHandle)
    std::string albedo_path{""};
    std::string normal_path{""};
    std::string metallic_path{""};
    std::string roughness_path{""};
    std::string ao_path{""};

    // Legacy fallback values (used if no MaterialHandle and no texture paths)
    glm::vec3 albedo_color{1.0f};
    float metallic{0.0f};
    float roughness{0.5f};

    // ==========================================================================
    // Serialization
    // ==========================================================================
    friend void to_json(nlohmann::json& j, const MeshComponent& c) {
        j = nlohmann::json{{"mesh_type", static_cast<int>(c.mesh_type)},
                           {"primitive_name", c.primitive_name},
                           {"model_index", c.model.index},
                           {"model_generation", c.model.generation},
                           {"material_index", c.material.index},
                           {"material_generation", c.material.generation},
                           // Legacy fields
                           {"mesh_path", c.mesh_path},
                           {"albedo_path", c.albedo_path},
                           {"normal_path", c.normal_path},
                           {"metallic_path", c.metallic_path},
                           {"roughness_path", c.roughness_path},
                           {"ao_path", c.ao_path},
                           {"albedo_color", TransformComponent::vec3_to_json(c.albedo_color)},
                           {"metallic", c.metallic},
                           {"roughness", c.roughness}};
    }

    friend void from_json(const nlohmann::json& j, MeshComponent& c) {
        // New fields
        c.mesh_type = static_cast<MeshType>(j.value("mesh_type", 0));
        c.primitive_name = j.value("primitive_name", "cube");
        c.model.index = j.value("model_index", 0u);
        c.model.generation = j.value("model_generation", 0u);
        c.material.index = j.value("material_index", 0u);
        c.material.generation = j.value("material_generation", 0u);

        // Legacy fields
        c.mesh_path = j.value("mesh_path", "cube");
        c.albedo_path = j.value("albedo_path", "");
        c.normal_path = j.value("normal_path", "");
        c.metallic_path = j.value("metallic_path", "");
        c.roughness_path = j.value("roughness_path", "");
        c.ao_path = j.value("ao_path", "");

        if (j.contains("albedo_color"))
            c.albedo_color = TransformComponent::vec3_from_json(j["albedo_color"]);
        c.metallic = j.value("metallic", 0.0f);
        c.roughness = j.value("roughness", 0.5f);

        // Migration: if mesh_path is set but primitive_name isn't, use mesh_path
        if (c.primitive_name == "cube" && c.mesh_path != "cube") {
            c.primitive_name = c.mesh_path;
        }
    }
};

// ==========================================
// Light Component
// ==========================================
enum class LightType { Directional, Point };

struct LightComponent {
    LightType type{LightType::Point};
    glm::vec3 color{1.0f};
    float intensity{1.0f};
    float range{10.0f}; // For point lights

    // Serialization
    friend void to_json(nlohmann::json& j, const LightComponent& c) {
        j = nlohmann::json{{"type", static_cast<int>(c.type)},
                           {"color", TransformComponent::vec3_to_json(c.color)},
                           {"intensity", c.intensity},
                           {"range", c.range}};
    }
    friend void from_json(const nlohmann::json& j, LightComponent& c) {
        c.type = static_cast<LightType>(j.value("type", 1));
        if (j.contains("color"))
            c.color = TransformComponent::vec3_from_json(j["color"]);
        c.intensity = j.value("intensity", 1.0f);
        c.range = j.value("range", 10.0f);
    }
};

} // namespace hz
