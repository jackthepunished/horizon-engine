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

// ==========================================
// Camera Component
// ==========================================
struct CameraComponent {
    float fov{45.0f};
    float near_plane{0.1f};
    float far_plane{1000.0f};
    bool primary{true};

    // Serialization
    friend void to_json(nlohmann::json& j, const CameraComponent& c) {
        j = nlohmann::json{{"fov", c.fov},
                           {"near_plane", c.near_plane},
                           {"far_plane", c.far_plane},
                           {"primary", c.primary}};
    }
    friend void from_json(const nlohmann::json& j, CameraComponent& c) {
        c.fov = j.value("fov", 45.0f);
        c.near_plane = j.value("near_plane", 0.1f);
        c.far_plane = j.value("far_plane", 1000.0f);
        c.primary = j.value("primary", true);
    }
};

// ==========================================
// Physics Components
// ==========================================
struct RigidBodyComponent {
    enum class BodyType { Static, Dynamic, Kinematic };
    BodyType type{BodyType::Static};
    float mass{1.0f};
    bool fixed_rotation{false};

    // Runtime data (not serialized)
    // We store the ID here to link ECS entity to Jolt body
    // In a real engine, we might want a separate system to map Entity <-> BodyID
    // to avoid storing runtime data in components, but this is simple and effective.
    //
    // Note: Using std::unique_ptr with type-erased deleter to avoid Jolt header dependency
    // while still ensuring proper cleanup when entity is destroyed.
    std::unique_ptr<void, void (*)(void*)> runtime_body{
        nullptr, [](void* p) {
            // Intentionally empty - PhysicsBodyID is trivially destructible
            // The actual physics body is managed by PhysicsWorld
            delete static_cast<char*>(p); // Safe deletion for any trivially destructible type
        }};
    bool created{false}; // Flag to check if body needs creation

    // Helper to safely access the body ID
    template <typename T>
    [[nodiscard]] T* get_body_id() const {
        return static_cast<T*>(runtime_body.get());
    }

    // Helper to set the body ID (takes ownership)
    template <typename T>
    void set_body_id(T* body_id) {
        runtime_body.reset(body_id);
    }

    // Serialization
    friend void to_json(nlohmann::json& j, const RigidBodyComponent& c) {
        j = nlohmann::json{{"type", static_cast<int>(c.type)},
                           {"mass", c.mass},
                           {"fixed_rotation", c.fixed_rotation}};
    }
    friend void from_json(const nlohmann::json& j, RigidBodyComponent& c) {
        c.type = static_cast<BodyType>(j.value("type", 0));
        c.mass = j.value("mass", 1.0f);
        c.fixed_rotation = j.value("fixed_rotation", false);
    }
};

struct BoxColliderComponent {
    glm::vec3 half_extents{0.5f};
    glm::vec3 offset{0.0f};

    friend void to_json(nlohmann::json& j, const BoxColliderComponent& c) {
        j = nlohmann::json{{"half_extents", TransformComponent::vec3_to_json(c.half_extents)},
                           {"offset", TransformComponent::vec3_to_json(c.offset)}};
    }
    friend void from_json(const nlohmann::json& j, BoxColliderComponent& c) {
        if (j.contains("half_extents"))
            c.half_extents = TransformComponent::vec3_from_json(j["half_extents"]);
        if (j.contains("offset"))
            c.offset = TransformComponent::vec3_from_json(j["offset"]);
    }
};

struct CapsuleColliderComponent {
    float radius{0.5f};
    float half_height{0.5f}; // Cylinder half-height. Total height = 2*half_height + 2*radius
    glm::vec3 offset{0.0f};

    friend void to_json(nlohmann::json& j, const CapsuleColliderComponent& c) {
        j = nlohmann::json{{"radius", c.radius},
                           {"half_height", c.half_height},
                           {"offset", TransformComponent::vec3_to_json(c.offset)}};
    }
    friend void from_json(const nlohmann::json& j, CapsuleColliderComponent& c) {
        c.radius = j.value("radius", 0.5f);
        c.half_height = j.value("half_height", 0.5f);
        if (j.contains("offset"))
            c.offset = TransformComponent::vec3_from_json(j["offset"]);
    }
};

struct LifetimeComponent {
    float time_remaining{1.0f};
};

// ==========================================
// IK Component
// ==========================================
struct IKTargetComponent {
    // Bone IDs for IK chain (e.g., shoulder, elbow, hand)
    i32 root_bone_id{-1};
    i32 mid_bone_id{-1};
    i32 end_bone_id{-1};

    // Target position in world space
    glm::vec3 target_position{0.0f};

    // Pole vector for controlling bend direction
    glm::vec3 pole_vector{0.0f, 0.0f, -1.0f};

    // Weight for blending IK with animation [0-1]
    float weight{1.0f};

    // Is IK active?
    bool enabled{true};
};

} // namespace hz
