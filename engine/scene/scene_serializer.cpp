#include "scene_serializer.hpp"

#include "components.hpp"
#include "engine/core/log.hpp"

#include <fstream>

#include <nlohmann/json.hpp>

// Explicitly use the macros if they are not in namespace
// (They usually are #define HZ_LOG_ERROR ...)

namespace hz {

SceneSerializer::SceneSerializer(Scene& scene) : m_scene(scene) {}

void SceneSerializer::serialize(const std::filesystem::path& path) {
    nlohmann::json root;
    root["entities"] = nlohmann::json::array();

    auto view = m_scene.registry().view<hz::Entity>();
    view.each([&](auto entity) {
        nlohmann::json entity_json;
        entity_json["id"] = static_cast<uint32_t>(entity); // For debugging

        // Serialize Components (manual mapping for now)
        // Check Tag
        if (auto* tc = m_scene.registry().try_get<TagComponent>(entity)) {
            entity_json["TagComponent"] = *tc;
        }

        if (auto* trc = m_scene.registry().try_get<TransformComponent>(entity)) {
            entity_json["TransformComponent"] = *trc;
        }

        if (auto* mc = m_scene.registry().try_get<MeshComponent>(entity)) {
            entity_json["MeshComponent"] = *mc;
        }

        if (auto* lc = m_scene.registry().try_get<LightComponent>(entity)) {
            entity_json["LightComponent"] = *lc;
        }

        root["entities"].push_back(entity_json);
    });

    std::ofstream file(path);
    if (!file.is_open()) {
        HZ_ERROR("Failed to open file for writing: {}", path.string());
        return;
    }
    file << root.dump(4);
}

bool SceneSerializer::deserialize(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        HZ_ERROR("Failed to open file for reading: {}", path.string());
        return false;
    }

    nlohmann::json root;
    try {
        file >> root;
    } catch (const nlohmann::json::parse_error& e) {
        HZ_ERROR("JSON Parse Error: {}", e.what());
        return false;
    }

    m_scene.clear();

    auto entities = root["entities"];
    for (auto& entity_json : entities) {
        auto entity = m_scene.create_entity();

        if (entity_json.contains("TagComponent")) {
            auto& tc = m_scene.registry().emplace<TagComponent>(entity);
            entity_json["TagComponent"].get_to(tc);
        }

        if (entity_json.contains("TransformComponent")) {
            auto& trc = m_scene.registry().emplace<TransformComponent>(entity);
            entity_json["TransformComponent"].get_to(trc);
        }

        if (entity_json.contains("MeshComponent")) {
            auto& mc = m_scene.registry().emplace<MeshComponent>(entity);
            entity_json["MeshComponent"].get_to(mc);
        }

        if (entity_json.contains("LightComponent")) {
            auto& lc = m_scene.registry().emplace<LightComponent>(entity);
            entity_json["LightComponent"].get_to(lc);
        }
    }

    HZ_LOG_INFO("Deserialized scene from: {}", path.string());
    return true;
}

} // namespace hz
