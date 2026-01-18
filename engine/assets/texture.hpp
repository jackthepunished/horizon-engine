#pragma once

/**
 * @file texture.hpp
 * @brief OpenGL texture wrapper with image loading
 */

#include "asset_handle.hpp"
#include "engine/core/log.hpp"
#include "engine/core/types.hpp"

#include <string>
#include <string_view>

namespace hz {

/**
 * @brief Texture format
 */
enum class TextureFormat : u8 { R8, RG8, RGB8, RGBA8, SRGB8, SRGBA8 };

/**
 * @brief Texture filter mode
 */
enum class TextureFilter : u8 { Nearest, Linear, NearestMipmap, LinearMipmap };

/**
 * @brief Texture wrap mode
 */
enum class TextureWrap : u8 { Repeat, MirroredRepeat, ClampToEdge };

/**
 * @brief Texture creation parameters
 */
struct TextureParams {
    TextureFilter min_filter{TextureFilter::LinearMipmap};
    TextureFilter mag_filter{TextureFilter::Linear};
    TextureWrap wrap_s{TextureWrap::Repeat};
    TextureWrap wrap_t{TextureWrap::Repeat};
    bool generate_mipmaps{true};
    bool srgb{true};
    bool flip_y{false}; // Set to false for GLTF textures (GLTF uses OpenGL UV convention)
};

/**
 * @brief OpenGL texture wrapper
 */
class Texture {
public:
    Texture() = default;
    ~Texture() noexcept;

    HZ_NON_COPYABLE(Texture);

    Texture(Texture&& other) noexcept;
    Texture& operator=(Texture&& other) noexcept;

    /**
     * @brief Load texture from file
     */
    [[nodiscard]] static Texture load_from_file(std::string_view path,
                                                const TextureParams& params = {});

    /**
     * @brief Create texture from raw data
     */
    [[nodiscard]] static Texture create(u32 width, u32 height, TextureFormat format,
                                        const void* data, const TextureParams& params = {});

    /**
     * @brief Load texture from memory buffer (e.g., embedded FBX texture)
     */
    [[nodiscard]] static Texture load_from_memory(const unsigned char* data, size_t size,
                                                  const TextureParams& params = {});

    /**
     * @brief Bind texture to a texture unit
     */
    void bind(u32 unit = 0) const;

    /**
     * @brief Unbind texture
     */
    static void unbind(u32 unit = 0);

    /**
     * @brief Check if texture is valid
     */
    [[nodiscard]] bool is_valid() const noexcept { return m_id != 0; }

    /**
     * @brief Get OpenGL texture ID
     */
    [[nodiscard]] u32 id() const noexcept { return m_id; }

    /**
     * @brief Get texture dimensions
     */
    [[nodiscard]] u32 width() const noexcept { return m_width; }
    [[nodiscard]] u32 height() const noexcept { return m_height; }

    /**
     * @brief Get file path (if loaded from file)
     */
    [[nodiscard]] const std::string& path() const noexcept { return m_path; }

private:
    u32 m_id{0};
    u32 m_width{0};
    u32 m_height{0};
    TextureFormat m_format{TextureFormat::RGBA8};
    std::string m_path;
};

} // namespace hz
