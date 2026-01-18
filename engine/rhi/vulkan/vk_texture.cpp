#include "vk_texture.hpp"

#include "vk_device.hpp"

namespace hz::rhi::vk {

// ============================================================================
// VulkanTexture Implementation
// ============================================================================

VulkanTexture::VulkanTexture(VulkanDevice& device, const TextureDesc& desc)
    : m_device(device)
    , m_type(desc.type)
    , m_format(desc.format)
    , m_width(desc.width)
    , m_height(desc.height)
    , m_depth(desc.depth)
    , m_mip_levels(desc.mip_levels)
    , m_array_layers(desc.array_layers)
    , m_sample_count(desc.sample_count)
    , m_usage(desc.usage)
    , m_owns_image(true) {

    // Calculate mip levels if requested
    if (m_mip_levels == 0) {
        m_mip_levels = TextureDesc::calculate_mip_levels(m_width, m_height, m_depth);
    }

    // Create image
    VkImageCreateInfo image_info{};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = to_vk_image_type(m_type);
    image_info.format = to_vk_format(m_format);
    image_info.extent.width = m_width;
    image_info.extent.height = m_height;
    image_info.extent.depth = m_depth;
    image_info.mipLevels = m_mip_levels;
    image_info.arrayLayers = m_array_layers;
    image_info.samples = static_cast<VkSampleCountFlagBits>(m_sample_count);
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.usage = to_vk_image_usage(m_usage);
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    // Handle cubemap flag
    if (m_type == TextureType::TextureCube || m_type == TextureType::TextureCubeArray) {
        image_info.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    }

    // Always enable transfer operations for flexibility
    image_info.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    // VMA allocation
    VmaAllocationCreateInfo alloc_info{};
    alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    VkResult result = vmaCreateImage(m_device.allocator(), &image_info, &alloc_info, &m_image,
                                     &m_allocation, nullptr);

    if (result != VK_SUCCESS) {
        HZ_LOG_ERROR("Failed to create Vulkan image: {}", vk_result_string(result));
        return;
    }

    // Set debug name
    if (desc.debug_name) {
        m_device.set_debug_name(reinterpret_cast<u64>(m_image), desc.debug_name);
    }

    HZ_LOG_DEBUG("Created Vulkan texture: {}x{}x{}, format={}, mips={}, layers={}", m_width,
                 m_height, m_depth, static_cast<u32>(m_format), m_mip_levels, m_array_layers);
}

VulkanTexture::VulkanTexture(VulkanDevice& device, VkImage image, const TextureDesc& desc,
                             bool owns_image)
    : m_device(device)
    , m_image(image)
    , m_type(desc.type)
    , m_format(desc.format)
    , m_width(desc.width)
    , m_height(desc.height)
    , m_depth(desc.depth)
    , m_mip_levels(desc.mip_levels)
    , m_array_layers(desc.array_layers)
    , m_sample_count(desc.sample_count)
    , m_usage(desc.usage)
    , m_owns_image(owns_image) {

    // Set debug name
    if (desc.debug_name) {
        m_device.set_debug_name(reinterpret_cast<u64>(m_image), desc.debug_name);
    }
}

VulkanTexture::~VulkanTexture() {
    if (m_image != VK_NULL_HANDLE && m_owns_image) {
        m_device.defer_deletion(m_image, m_allocation);
        m_image = VK_NULL_HANDLE;
        m_allocation = VK_NULL_HANDLE;
    }
}

// ============================================================================
// VulkanTextureView Implementation
// ============================================================================

VulkanTextureView::VulkanTextureView(VulkanDevice& device, const TextureViewDesc& desc)
    : m_device(device)
    , m_texture(static_cast<const VulkanTexture*>(desc.texture))
    , m_view_type(desc.view_type)
    , m_format(desc.format != Format::Unknown ? desc.format : desc.texture->format())
    , m_base_mip_level(desc.base_mip_level)
    , m_mip_level_count(desc.mip_level_count)
    , m_base_array_layer(desc.base_array_layer)
    , m_array_layer_count(desc.array_layer_count) {

    // Resolve "remaining" counts
    if (m_mip_level_count == UINT32_MAX) {
        m_mip_level_count = m_texture->mip_levels() - m_base_mip_level;
    }
    if (m_array_layer_count == UINT32_MAX) {
        m_array_layer_count = m_texture->array_layers() - m_base_array_layer;
    }

    VkImageViewCreateInfo view_info{};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = m_texture->image();
    view_info.viewType = to_vk_image_view_type(m_view_type);
    view_info.format = to_vk_format(m_format);

    // Component swizzle (identity)
    view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    // Subresource range
    view_info.subresourceRange.aspectMask = get_image_aspect(m_format);
    view_info.subresourceRange.baseMipLevel = m_base_mip_level;
    view_info.subresourceRange.levelCount = m_mip_level_count;
    view_info.subresourceRange.baseArrayLayer = m_base_array_layer;
    view_info.subresourceRange.layerCount = m_array_layer_count;

    VkResult result = vkCreateImageView(m_device.device(), &view_info, nullptr, &m_image_view);
    if (result != VK_SUCCESS) {
        HZ_LOG_ERROR("Failed to create Vulkan image view: {}", vk_result_string(result));
        return;
    }

    // Set debug name
    if (desc.debug_name) {
        m_device.set_debug_name(reinterpret_cast<u64>(m_image_view), desc.debug_name);
    }
}

VulkanTextureView::~VulkanTextureView() {
    if (m_image_view != VK_NULL_HANDLE) {
        m_device.defer_deletion(m_image_view);
        m_image_view = VK_NULL_HANDLE;
    }
}

} // namespace hz::rhi::vk
