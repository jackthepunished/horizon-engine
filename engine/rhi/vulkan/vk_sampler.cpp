#include "vk_sampler.hpp"

#include "vk_device.hpp"

namespace hz::rhi::vk {

VulkanSampler::VulkanSampler(VulkanDevice& device, const SamplerDesc& desc) : m_device(device) {

    VkSamplerCreateInfo sampler_info{};
    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_info.magFilter = to_vk_filter(desc.mag_filter);
    sampler_info.minFilter = to_vk_filter(desc.min_filter);
    sampler_info.mipmapMode = to_vk_mipmap_mode(desc.mipmap_mode);
    sampler_info.addressModeU = to_vk_address_mode(desc.address_u);
    sampler_info.addressModeV = to_vk_address_mode(desc.address_v);
    sampler_info.addressModeW = to_vk_address_mode(desc.address_w);
    sampler_info.mipLodBias = desc.mip_lod_bias;
    sampler_info.anisotropyEnable = desc.anisotropy_enable ? VK_TRUE : VK_FALSE;
    sampler_info.maxAnisotropy = desc.max_anisotropy;
    sampler_info.compareEnable = desc.compare_enable ? VK_TRUE : VK_FALSE;
    sampler_info.compareOp = to_vk_compare_op(desc.compare_op);
    sampler_info.minLod = desc.min_lod;
    sampler_info.maxLod = desc.max_lod;
    sampler_info.borderColor = to_vk_border_color(desc.border_color);
    sampler_info.unnormalizedCoordinates = VK_FALSE;

    VkResult result = vkCreateSampler(m_device.device(), &sampler_info, nullptr, &m_sampler);
    if (result != VK_SUCCESS) {
        HZ_LOG_ERROR("Failed to create Vulkan sampler: {}", vk_result_string(result));
        return;
    }

    // Set debug name
    if (desc.debug_name) {
        m_device.set_debug_name(reinterpret_cast<u64>(m_sampler), desc.debug_name);
    }
}

VulkanSampler::~VulkanSampler() {
    if (m_sampler != VK_NULL_HANDLE) {
        m_device.defer_deletion(m_sampler);
        m_sampler = VK_NULL_HANDLE;
    }
}

} // namespace hz::rhi::vk
