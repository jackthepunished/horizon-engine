#pragma once

/**
 * @file texture.hpp
 * @brief Texture asset with RHI GPU resource management
 */

#include "asset_handle.hpp"
#include "engine/core/log.hpp"
#include "engine/core/types.hpp"
#include "engine/rhi/rhi_device.hpp"
#include "engine/rhi/rhi_resources.hpp"

#include <memory>
#include <string>
#include <string_view>

namespace hz {

/**
 * @brief Texture format (CPU-side channel description)
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
 * @brief Texture asset with RHI GPU resource management
 *
 * Load pixel data from file using stb_image, then call upload_to_gpu()
 * to create device-local RHI texture/view resources.
 */
class Texture {
public:
    Texture() = default;
    ~Texture() noexcept;

    HZ_NON_COPYABLE(Texture);

    Texture(Texture&& other) noexcept;
    Texture& operator=(Texture&& other) noexcept;

    // =========================================================================
    // Loading (CPU pixel data)
    // =========================================================================

    [[nodiscard]] static Texture load_from_file(std::string_view path,
                                                const TextureParams& params = {});

    [[nodiscard]] static Texture create(u32 width, u32 height, TextureFormat format,
                                        const void* data, const TextureParams& params = {});

    [[nodiscard]] static Texture load_from_memory(const unsigned char* data, size_t size,
                                                  const TextureParams& params = {});

    // =========================================================================
    // GPU Resource Management
    // =========================================================================

    /**
     * @brief Upload CPU pixel data to a device-local RHI texture
     * @param device RHI device to create resources on
     *
     * Creates an rhi::Texture + rhi::TextureView. If generate_mipmaps was
     * requested in TextureParams, mipmaps are generated on the GPU.
     */
    void upload_to_gpu(rhi::Device& device);

    /**
     * @brief Check if GPU resources have been created
     */
    [[nodiscard]] bool is_uploaded() const noexcept { return m_rhi_texture != nullptr; }

    /**
     * @brief Release GPU resources
     */
    void release_gpu_resources();

    /**
     * @brief Get the RHI texture (nullptr if not uploaded)
     */
    [[nodiscard]] rhi::Texture* rhi_texture() const noexcept { return m_rhi_texture.get(); }

    /**
     * @brief Get the RHI texture view (nullptr if not uploaded)
     */
    [[nodiscard]] rhi::TextureView* rhi_view() const noexcept { return m_rhi_view.get(); }

    // =========================================================================
    // Accessors
    // =========================================================================

    [[nodiscard]] bool is_valid() const noexcept { return m_width > 0; }

    [[nodiscard]] u32 width() const noexcept { return m_width; }
    [[nodiscard]] u32 height() const noexcept { return m_height; }

    [[nodiscard]] const std::string& path() const noexcept { return m_path; }
    [[nodiscard]] TextureFormat format() const noexcept { return m_format; }

private:
    u32 m_width{0};
    u32 m_height{0};
    i32 m_channels{0}; // Original channel count from stb_image
    TextureFormat m_format{TextureFormat::RGBA8};
    TextureParams m_params{};
    std::string m_path;

    // CPU pixel data (kept until upload, or always for re-upload on resize)
    std::vector<u8> m_pixel_data;

    // GPU resources
    std::unique_ptr<rhi::Texture> m_rhi_texture;
    std::unique_ptr<rhi::TextureView> m_rhi_view;
};

} // namespace hz
