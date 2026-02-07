#include "texture.hpp"

#include <stb_image.h>

namespace hz {

// ============================================================================
// Destruction & Move
// ============================================================================

Texture::~Texture() noexcept = default;

Texture::Texture(Texture&& other) noexcept = default;
Texture& Texture::operator=(Texture&& other) noexcept = default;

// ============================================================================
// Helper: map TextureFormat to rhi::Format
// ============================================================================

static rhi::Format to_rhi_format(TextureFormat fmt, bool srgb) {
    switch (fmt) {
    case TextureFormat::R8:
        return rhi::Format::R8_UNORM;
    case TextureFormat::RG8:
        return rhi::Format::RG8_UNORM;
    case TextureFormat::RGB8:
        // RGB8 doesn't exist in Vulkan; we always load as RGBA8
        return srgb ? rhi::Format::RGBA8_SRGB : rhi::Format::RGBA8_UNORM;
    case TextureFormat::RGBA8:
        return srgb ? rhi::Format::RGBA8_SRGB : rhi::Format::RGBA8_UNORM;
    case TextureFormat::SRGB8:
        return rhi::Format::RGBA8_SRGB;
    case TextureFormat::SRGBA8:
        return rhi::Format::RGBA8_SRGB;
    }
    return rhi::Format::RGBA8_UNORM;
}

// ============================================================================
// Loading from file
// ============================================================================

Texture Texture::load_from_file(std::string_view path, const TextureParams& params) {
    Texture tex;
    tex.m_path = std::string(path);
    tex.m_params = params;

    stbi_set_flip_vertically_on_load(params.flip_y ? 1 : 0);

    int w = 0, h = 0, channels = 0;
    // Always request 4 channels (RGBA) for Vulkan compatibility
    unsigned char* pixels = stbi_load(path.data(), &w, &h, &channels, 4);

    if (!pixels) {
        HZ_LOG_ERROR("Failed to load texture: {}", path);
        return tex;
    }

    tex.m_width = static_cast<u32>(w);
    tex.m_height = static_cast<u32>(h);
    tex.m_channels = channels;

    // We requested 4 channels, so it's always RGBA8
    tex.m_format = params.srgb ? TextureFormat::SRGBA8 : TextureFormat::RGBA8;

    size_t byte_count = static_cast<size_t>(w) * static_cast<size_t>(h) * 4;
    tex.m_pixel_data.assign(pixels, pixels + byte_count);
    stbi_image_free(pixels);

    HZ_LOG_INFO("Loaded texture: {} ({}x{}, {} channels)", path, w, h, channels);
    return tex;
}

// ============================================================================
// Loading from memory
// ============================================================================

Texture Texture::load_from_memory(const unsigned char* data, size_t size,
                                  const TextureParams& params) {
    Texture tex;
    tex.m_params = params;

    stbi_set_flip_vertically_on_load(params.flip_y ? 1 : 0);

    int w = 0, h = 0, channels = 0;
    unsigned char* pixels =
        stbi_load_from_memory(data, static_cast<int>(size), &w, &h, &channels, 4);

    if (!pixels) {
        HZ_LOG_ERROR("Failed to load texture from memory");
        return tex;
    }

    tex.m_width = static_cast<u32>(w);
    tex.m_height = static_cast<u32>(h);
    tex.m_channels = channels;
    tex.m_format = params.srgb ? TextureFormat::SRGBA8 : TextureFormat::RGBA8;

    size_t byte_count = static_cast<size_t>(w) * static_cast<size_t>(h) * 4;
    tex.m_pixel_data.assign(pixels, pixels + byte_count);
    stbi_image_free(pixels);

    return tex;
}

// ============================================================================
// Create from raw data
// ============================================================================

Texture Texture::create(u32 width, u32 height, TextureFormat format, const void* data,
                        const TextureParams& params) {
    Texture tex;
    tex.m_width = width;
    tex.m_height = height;
    tex.m_format = format;
    tex.m_params = params;

    // Determine bytes per pixel for the given format
    u32 bpp = 4; // default RGBA
    switch (format) {
    case TextureFormat::R8:
        bpp = 1;
        break;
    case TextureFormat::RG8:
        bpp = 2;
        break;
    case TextureFormat::RGB8:
    case TextureFormat::SRGB8:
        bpp = 3;
        break;
    case TextureFormat::RGBA8:
    case TextureFormat::SRGBA8:
        bpp = 4;
        break;
    }

    if (data) {
        size_t byte_count = static_cast<size_t>(width) * height * bpp;
        const auto* src = static_cast<const u8*>(data);
        tex.m_pixel_data.assign(src, src + byte_count);
    }

    return tex;
}

// ============================================================================
// GPU Upload
// ============================================================================

void Texture::upload_to_gpu(rhi::Device& device) {
    if (m_pixel_data.empty() || m_width == 0 || m_height == 0) {
        HZ_LOG_WARN("Texture::upload_to_gpu called with no pixel data");
        return;
    }

    rhi::Format rhi_fmt = to_rhi_format(m_format, m_params.srgb);

    // Calculate mip levels if mipmaps are requested
    u32 mip_levels = 1;
    if (m_params.generate_mipmaps) {
        mip_levels = rhi::TextureDesc::calculate_mip_levels(m_width, m_height);
    }

    rhi::TextureDesc desc =
        rhi::TextureDesc::texture_2d(m_width, m_height, rhi_fmt,
                                     rhi::TextureUsage::Sampled | rhi::TextureUsage::TransferDst |
                                         rhi::TextureUsage::TransferSrc,
                                     mip_levels);
    desc.debug_name = m_path.empty() ? "Texture" : m_path.c_str();

    m_rhi_texture = device.create_texture(desc);
    if (!m_rhi_texture) {
        HZ_LOG_ERROR("Failed to create RHI texture for: {}", m_path);
        return;
    }

    // Upload pixel data to mip 0
    device.update_texture(*m_rhi_texture, m_pixel_data.data(),
                          static_cast<u64>(m_pixel_data.size()));

    // Generate mipmaps on GPU if requested
    if (m_params.generate_mipmaps && mip_levels > 1) {
        device.generate_mipmaps(*m_rhi_texture);
    }

    // Create a default view covering all mip levels
    m_rhi_view = device.create_texture_view(*m_rhi_texture,
                                            m_path.empty() ? "Texture View" : m_path.c_str());
    if (!m_rhi_view) {
        HZ_LOG_ERROR("Failed to create RHI texture view for: {}", m_path);
    }
}

void Texture::release_gpu_resources() {
    m_rhi_view.reset();
    m_rhi_texture.reset();
}

} // namespace hz
