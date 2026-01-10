#pragma once

/**
 * @file asset_registry.hpp
 * @brief Central registry for asset management
 */

#include "engine/core/log.hpp"
#include "engine/core/types.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <engine/assets/asset_handle.hpp>
#include <engine/assets/model.hpp>
#include <engine/assets/texture.hpp>
#include <engine/audio/audio_engine.hpp>

namespace hz {

/**
 * @brief Central asset registry with handle-based access
 */
class AssetRegistry {
public:
    AssetRegistry() = default;
    ~AssetRegistry() = default;

    HZ_NON_COPYABLE(AssetRegistry);
    HZ_NON_MOVABLE(AssetRegistry);

    // ========================================================================
    // Texture Management
    // ========================================================================

    /**
     * @brief Load or get cached texture
     */
    TextureHandle load_texture(std::string_view path, const TextureParams& params = {});

    /**
     * @brief Get texture by handle
     */
    [[nodiscard]] Texture* get_texture(TextureHandle handle);
    [[nodiscard]] const Texture* get_texture(TextureHandle handle) const;

    /**
     * @brief Reload a texture from disk
     */
    bool reload_texture(TextureHandle handle);

    // ========================================================================
    // Model Management
    // ========================================================================

    /**
     * @brief Load or get cached model
     */
    ModelHandle load_model(std::string_view path);

    /**
     * @brief Get model by handle
     */
    [[nodiscard]] Model* get_model(ModelHandle handle);
    [[nodiscard]] const Model* get_model(ModelHandle handle) const;

    /**
     * @brief Reload a model from disk
     */
    bool reload_model(ModelHandle handle);

    // ========================================================================
    // Sound Management
    // ========================================================================

    /**
     * @brief Load or get cached sound
     */
    SoundHandle load_sound(const std::string& path, AudioSystem& audio);

    // ========================================================================
    // Utility
    // ========================================================================

    /**
     * @brief Reload all modified assets
     */
    void reload_all();

    /**
     * @brief Clear all assets
     */
    void clear();

    /**
     * @brief Get statistics
     */
    [[nodiscard]] size_t texture_count() const { return m_textures.size(); }
    [[nodiscard]] size_t model_count() const { return m_models.size(); }

private:
    template <typename T, typename Handle>
    struct AssetSlot {
        T asset;
        u32 generation{1};
        std::string path;
    };

    std::vector<AssetSlot<Texture, TextureHandle>> m_textures;
    std::unordered_map<std::string, u32> m_texture_path_to_index;

    std::vector<AssetSlot<Model, ModelHandle>> m_models;
    std::unordered_map<std::string, u32> m_model_path_to_index;

    // Sound cache (path -> handle)
    std::unordered_map<std::string, SoundHandle> m_loaded_sounds;
};

} // namespace hz
