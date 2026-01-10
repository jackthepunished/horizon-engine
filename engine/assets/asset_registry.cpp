#include "asset_registry.hpp"

namespace hz {

// ============================================================================
// Texture Management
// ============================================================================

TextureHandle AssetRegistry::load_texture(std::string_view path, const TextureParams& params) {
    std::string path_str(path);

    // Check if already loaded
    auto it = m_texture_path_to_index.find(path_str);
    if (it != m_texture_path_to_index.end()) {
        auto& slot = m_textures[it->second];
        return {it->second, slot.generation};
    }

    // Load new texture
    Texture tex = Texture::load_from_file(path, params);
    if (!tex.is_valid()) {
        return TextureHandle::invalid();
    }

    u32 index = static_cast<u32>(m_textures.size());
    m_textures.push_back({std::move(tex), 1, path_str});
    m_texture_path_to_index[path_str] = index;

    return {index, 1};
}

Texture* AssetRegistry::get_texture(TextureHandle handle) {
    if (handle.index >= m_textures.size())
        return nullptr;
    auto& slot = m_textures[handle.index];
    if (slot.generation != handle.generation)
        return nullptr;
    return &slot.asset;
}

const Texture* AssetRegistry::get_texture(TextureHandle handle) const {
    if (handle.index >= m_textures.size())
        return nullptr;
    const auto& slot = m_textures[handle.index];
    if (slot.generation != handle.generation)
        return nullptr;
    return &slot.asset;
}

bool AssetRegistry::reload_texture(TextureHandle handle) {
    if (handle.index >= m_textures.size())
        return false;
    auto& slot = m_textures[handle.index];
    if (slot.generation != handle.generation)
        return false;

    Texture new_tex = Texture::load_from_file(slot.path);
    if (!new_tex.is_valid())
        return false;

    slot.asset = std::move(new_tex);
    slot.generation++;
    HZ_ENGINE_INFO("Reloaded texture: {}", slot.path);
    return true;
}

// ============================================================================
// Model Management
// ============================================================================

ModelHandle AssetRegistry::load_model(std::string_view path) {
    std::string path_str(path);

    auto it = m_model_path_to_index.find(path_str);
    if (it != m_model_path_to_index.end()) {
        auto& slot = m_models[it->second];
        return {it->second, slot.generation};
    }

    Model model = Model::load_from_obj(path);
    if (!model.is_valid()) {
        return ModelHandle::invalid();
    }

    u32 index = static_cast<u32>(m_models.size());
    m_models.push_back({std::move(model), 1, path_str});
    m_model_path_to_index[path_str] = index;

    return {index, 1};
}

Model* AssetRegistry::get_model(ModelHandle handle) {
    if (handle.index >= m_models.size())
        return nullptr;
    auto& slot = m_models[handle.index];
    if (slot.generation != handle.generation)
        return nullptr;
    return &slot.asset;
}

const Model* AssetRegistry::get_model(ModelHandle handle) const {
    if (handle.index >= m_models.size())
        return nullptr;
    const auto& slot = m_models[handle.index];
    if (slot.generation != handle.generation)
        return nullptr;
    return &slot.asset;
}

bool AssetRegistry::reload_model(ModelHandle handle) {
    if (handle.index >= m_models.size())
        return false;
    auto& slot = m_models[handle.index];
    if (slot.generation != handle.generation)
        return false;

    Model new_model = Model::load_from_obj(slot.path);
    if (!new_model.is_valid())
        return false;

    slot.asset = std::move(new_model);
    slot.generation++;
    HZ_ENGINE_INFO("Reloaded model: {}", slot.path);
    return true;
}

// ============================================================================
// Sound Management
// ============================================================================

SoundHandle AssetRegistry::load_sound(const std::string& path, AudioSystem& audio) {
    auto it = m_loaded_sounds.find(path);
    if (it != m_loaded_sounds.end()) {
        return it->second;
    }

    SoundHandle handle = audio.load_sound(path);
    if (handle.is_valid()) {
        m_loaded_sounds[path] = handle;
    }
    return handle;
}

// ============================================================================
// Utility
// ============================================================================

void AssetRegistry::reload_all() {
    for (u32 i = 0; i < m_textures.size(); ++i) {
        reload_texture({i, m_textures[i].generation});
    }
    for (u32 i = 0; i < m_models.size(); ++i) {
        reload_model({i, m_models[i].generation});
    }
}

void AssetRegistry::clear() {
    m_textures.clear();
    m_texture_path_to_index.clear();
    m_models.clear();
    m_model_path_to_index.clear();
    m_loaded_sounds.clear();
    HZ_ENGINE_INFO("Asset registry cleared");
}

} // namespace hz
