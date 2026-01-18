#pragma once

/**
 * @file vk_sync.hpp
 * @brief Vulkan Synchronization primitives
 *
 * Implements the RHI Fence and Semaphore interfaces for Vulkan.
 */

#include "../rhi_resources.hpp"
#include "vk_common.hpp"

namespace hz::rhi::vk {

class VulkanDevice;

// ============================================================================
// Vulkan Fence
// ============================================================================

/**
 * @brief Vulkan implementation of the Fence interface
 *
 * Wraps VkFence for CPU-GPU synchronization.
 */
class VulkanFence final : public Fence {
public:
    VulkanFence(VulkanDevice& device, bool signaled);
    ~VulkanFence() override;

    // ========================================================================
    // Fence Interface
    // ========================================================================

    [[nodiscard]] bool is_signaled() const override;
    [[nodiscard]] bool wait(u64 timeout_ns = UINT64_MAX) override;
    void reset() override;

    [[nodiscard]] u64 native_handle() const noexcept override {
        return reinterpret_cast<u64>(m_fence);
    }

    // ========================================================================
    // Vulkan-Specific Accessors
    // ========================================================================

    [[nodiscard]] VkFence fence() const noexcept { return m_fence; }

private:
    VulkanDevice& m_device;
    VkFence m_fence{VK_NULL_HANDLE};
};

// ============================================================================
// Vulkan Semaphore
// ============================================================================

/**
 * @brief Vulkan implementation of the Semaphore interface
 *
 * Wraps VkSemaphore for GPU-GPU synchronization.
 */
class VulkanSemaphore final : public Semaphore {
public:
    explicit VulkanSemaphore(VulkanDevice& device);
    ~VulkanSemaphore() override;

    [[nodiscard]] u64 native_handle() const noexcept override {
        return reinterpret_cast<u64>(m_semaphore);
    }

    // ========================================================================
    // Vulkan-Specific Accessors
    // ========================================================================

    [[nodiscard]] VkSemaphore semaphore() const noexcept { return m_semaphore; }

private:
    VulkanDevice& m_device;
    VkSemaphore m_semaphore{VK_NULL_HANDLE};
};

} // namespace hz::rhi::vk
