#include "vk_sync.hpp"

#include "vk_device.hpp"

namespace hz::rhi::vk {

// ============================================================================
// VulkanFence Implementation
// ============================================================================

VulkanFence::VulkanFence(VulkanDevice& device, bool signaled) : m_device(device) {

    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = signaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0;

    VkResult result = vkCreateFence(m_device.device(), &fence_info, nullptr, &m_fence);
    if (result != VK_SUCCESS) {
        HZ_LOG_ERROR("Failed to create Vulkan fence: {}", vk_result_string(result));
    }
}

VulkanFence::~VulkanFence() {
    if (m_fence != VK_NULL_HANDLE) {
        vkDestroyFence(m_device.device(), m_fence, nullptr);
        m_fence = VK_NULL_HANDLE;
    }
}

bool VulkanFence::is_signaled() const {
    VkResult result = vkGetFenceStatus(m_device.device(), m_fence);
    return result == VK_SUCCESS;
}

bool VulkanFence::wait(u64 timeout_ns) {
    VkResult result = vkWaitForFences(m_device.device(), 1, &m_fence, VK_TRUE, timeout_ns);
    return result == VK_SUCCESS;
}

void VulkanFence::reset() {
    vkResetFences(m_device.device(), 1, &m_fence);
}

// ============================================================================
// VulkanSemaphore Implementation
// ============================================================================

VulkanSemaphore::VulkanSemaphore(VulkanDevice& device) : m_device(device) {

    VkSemaphoreCreateInfo semaphore_info{};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkResult result = vkCreateSemaphore(m_device.device(), &semaphore_info, nullptr, &m_semaphore);
    if (result != VK_SUCCESS) {
        HZ_LOG_ERROR("Failed to create Vulkan semaphore: {}", vk_result_string(result));
    }
}

VulkanSemaphore::~VulkanSemaphore() {
    if (m_semaphore != VK_NULL_HANDLE) {
        vkDestroySemaphore(m_device.device(), m_semaphore, nullptr);
        m_semaphore = VK_NULL_HANDLE;
    }
}

} // namespace hz::rhi::vk
