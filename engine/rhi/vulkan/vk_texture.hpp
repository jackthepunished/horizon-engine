#pragma once

/**
 * @file vk_texture.hpp
 * @brief Vulkan Texture and TextureView implementations
 *
 * Implements the RHI Texture and TextureView interfaces for Vulkan.
 */

#include "../rhi_resources.hpp"
#include "vk_common.hpp"

namespace hz::rhi::vk {

class VulkanDevice;

// ============================================================================
// Vulkan Texture
// ============================================================================

/**
 * @brief Vulkan implementation of the Texture interface
 */
class VulkanTexture final : public Texture {
public:
    /**
     * @brief Create a new texture with a new VkImage
     */
    VulkanTexture(VulkanDevice& device, const TextureDesc& desc);

    /**
     * @brief Wrap an existing VkImage (e.g., from swapchain)
     */
    VulkanTexture(VulkanDevice& device, VkImage image, const TextureDesc& desc, bool owns_image);

    ~VulkanTexture() override;

    // ========================================================================
    // Texture Interface
    // ========================================================================

    [[nodiscard]] TextureType type() const noexcept override { return m_type; }
    [[nodiscard]] Format format() const noexcept override { return m_format; }
    [[nodiscard]] u32 width() const noexcept override { return m_width; }
    [[nodiscard]] u32 height() const noexcept override { return m_height; }
    [[nodiscard]] u32 depth() const noexcept override { return m_depth; }
    [[nodiscard]] u32 mip_levels() const noexcept override { return m_mip_levels; }
    [[nodiscard]] u32 array_layers() const noexcept override { return m_array_layers; }
    [[nodiscard]] u32 sample_count() const noexcept override { return m_sample_count; }
    [[nodiscard]] TextureUsage usage() const noexcept override { return m_usage; }

    [[nodiscard]] u64 native_handle() const noexcept override {
        return reinterpret_cast<u64>(m_image);
    }

    // ========================================================================
    // Vulkan-Specific Accessors
    // ========================================================================

    [[nodiscard]] VkImage image() const noexcept { return m_image; }
    [[nodiscard]] VmaAllocation allocation() const noexcept { return m_allocation; }
    [[nodiscard]] bool owns_image() const noexcept { return m_owns_image; }

private:
    VulkanDevice& m_device;

    VkImage m_image{VK_NULL_HANDLE};
    VmaAllocation m_allocation{VK_NULL_HANDLE};

    TextureType m_type{TextureType::Texture2D};
    Format m_format{Format::RGBA8_UNORM};
    u32 m_width{1};
    u32 m_height{1};
    u32 m_depth{1};
    u32 m_mip_levels{1};
    u32 m_array_layers{1};
    u32 m_sample_count{1};
    TextureUsage m_usage{TextureUsage::Sampled};

    bool m_owns_image{true}; ///< False for swapchain images
};

// ============================================================================
// Vulkan Texture View
// ============================================================================

/**
 * @brief Vulkan implementation of the TextureView interface
 */
class VulkanTextureView final : public TextureView {
public:
    VulkanTextureView(VulkanDevice& device, const TextureViewDesc& desc);
    ~VulkanTextureView() override;

    // ========================================================================
    // TextureView Interface
    // ========================================================================

    [[nodiscard]] const Texture& texture() const noexcept override { return *m_texture; }
    [[nodiscard]] TextureType view_type() const noexcept override { return m_view_type; }
    [[nodiscard]] Format format() const noexcept override { return m_format; }
    [[nodiscard]] u32 base_mip_level() const noexcept override { return m_base_mip_level; }
    [[nodiscard]] u32 mip_level_count() const noexcept override { return m_mip_level_count; }
    [[nodiscard]] u32 base_array_layer() const noexcept override { return m_base_array_layer; }
    [[nodiscard]] u32 array_layer_count() const noexcept override { return m_array_layer_count; }

    [[nodiscard]] u64 native_handle() const noexcept override {
        return reinterpret_cast<u64>(m_image_view);
    }

    // ========================================================================
    // Vulkan-Specific Accessors
    // ========================================================================

    [[nodiscard]] VkImageView image_view() const noexcept { return m_image_view; }

private:
    VulkanDevice& m_device;

    VkImageView m_image_view{VK_NULL_HANDLE};
    const VulkanTexture* m_texture{nullptr};

    TextureType m_view_type{TextureType::Texture2D};
    Format m_format{Format::RGBA8_UNORM};
    u32 m_base_mip_level{0};
    u32 m_mip_level_count{1};
    u32 m_base_array_layer{0};
    u32 m_array_layer_count{1};
};

} // namespace hz::rhi::vk
