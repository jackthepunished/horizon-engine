#pragma once

/**
 * @file vk_sampler.hpp
 * @brief Vulkan Sampler implementation
 *
 * Implements the RHI Sampler interface for Vulkan.
 */

#include "../rhi_resources.hpp"
#include "vk_common.hpp"

namespace hz::rhi::vk {

class VulkanDevice;

/**
 * @brief Vulkan implementation of the Sampler interface
 */
class VulkanSampler final : public Sampler {
public:
    VulkanSampler(VulkanDevice& device, const SamplerDesc& desc);
    ~VulkanSampler() override;

    [[nodiscard]] u64 native_handle() const noexcept override {
        return reinterpret_cast<u64>(m_sampler);
    }

    // ========================================================================
    // Vulkan-Specific Accessors
    // ========================================================================

    [[nodiscard]] VkSampler sampler() const noexcept { return m_sampler; }

private:
    VulkanDevice& m_device;
    VkSampler m_sampler{VK_NULL_HANDLE};
};

} // namespace hz::rhi::vk
