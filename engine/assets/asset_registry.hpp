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
#include <engine/assets/material.hpp>
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
    [[nodiscard]] TextureHandle load_texture(std::string_view path,
                                             const TextureParams& params = {});

    /**
     * @brief Get texture by handle
     */
    [[nodiscard]] Texture* get_texture(TextureHandle handle);
    [[nodiscard]] const Texture* get_texture(TextureHandle handle) const;

    /**
     * @brief Reload a texture from disk
     */
    [[nodiscard]] bool reload_texture(TextureHandle handle);

    /**
     * @brief Register an existing texture
     */
    [[nodiscard]] TextureHandle register_texture(Texture&& texture, const std::string& name);

    // ========================================================================
    // Model Management
    // ========================================================================

    /**
     * @brief Load or get cached model
     */
    [[nodiscard]] ModelHandle load_model(std::string_view path);

    /**
     * @brief Get model by handle
     */
    [[nodiscard]] Model* get_model(ModelHandle handle);
    [[nodiscard]] const Model* get_model(ModelHandle handle) const;

    /**
     * @brief Reload a model from disk
     */
    [[nodiscard]] bool reload_model(ModelHandle handle);

    /**
     * @brief Register an existing model
     */
    [[nodiscard]] ModelHandle register_model(Model&& model, const std::string& name);

    // ========================================================================
    // Material Management
    // ========================================================================

    /**
     * @brief Create or get a named material
     */
    [[nodiscard]] MaterialHandle create_material(const std::string& name, const Material& mat);

    /**
     * @brief Get material by handle
     */
    [[nodiscard]] Material* get_material(MaterialHandle handle);
    [[nodiscard]] const Material* get_material(MaterialHandle handle) const;

    /**
     * @brief Get a default white material
     */
    [[nodiscard]] MaterialHandle get_default_material();

    /**
     * @brief Get material by name
     */
    [[nodiscard]] MaterialHandle get_material_by_name(const std::string& name);

    // ========================================================================
    // Sound Management
    // ========================================================================

    /**
     * @brief Load or get cached sound
     */
    [[nodiscard]] SoundHandle load_sound(const std::string& path, AudioSystem& audio);

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
    [[nodiscard]] size_t material_count() const { return m_materials.size(); }

private:
    template <typename T, typename Handle>
    struct AssetSlot {
        T asset;
        u32 generation{1};
        std::string path;
    };

    std::vector<AssetSlot<Texture, TextureHandle>> m_textures;
    std::unordered_map<std::string, u32, TransparentStringHash, std::equal_to<>>
        m_texture_path_to_index;

    std::vector<AssetSlot<Model, ModelHandle>> m_models;
    std::unordered_map<std::string, u32, TransparentStringHash, std::equal_to<>>
        m_model_path_to_index;

    std::vector<AssetSlot<Material, MaterialHandle>> m_materials;
    std::unordered_map<std::string, u32, TransparentStringHash, std::equal_to<>>
        m_material_name_to_index;
    MaterialHandle m_default_material{};

    // Sound cache (path -> handle)
    std::unordered_map<std::string, SoundHandle, TransparentStringHash, std::equal_to<>>
        m_loaded_sounds;
};

} // namespace hz
