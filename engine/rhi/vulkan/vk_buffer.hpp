#pragma once

/**
 * @file vk_buffer.hpp
 * @brief Vulkan Buffer implementation
 *
 * Implements the RHI Buffer interface using VkBuffer with VMA for memory allocation.
 */

#include "../rhi_resources.hpp"
#include "vk_common.hpp"

namespace hz::rhi::vk {

class VulkanDevice;

/**
 * @brief Vulkan implementation of the Buffer interface
 */
class VulkanBuffer final : public Buffer {
public:
    VulkanBuffer(VulkanDevice& device, const BufferDesc& desc);
    ~VulkanBuffer() override;

    // ========================================================================
    // Buffer Interface
    // ========================================================================

    [[nodiscard]] u64 size() const noexcept override { return m_size; }
    [[nodiscard]] BufferUsage usage() const noexcept override { return m_usage; }
    [[nodiscard]] MemoryUsage memory_usage() const noexcept override { return m_memory_usage; }

    [[nodiscard]] void* map() override;
    void unmap() override;
    void flush(u64 offset = 0, u64 size = UINT64_MAX) override;
    void invalidate(u64 offset = 0, u64 size = UINT64_MAX) override;

    [[nodiscard]] u64 native_handle() const noexcept override {
        return reinterpret_cast<u64>(m_buffer);
    }

    // ========================================================================
    // Vulkan-Specific Accessors
    // ========================================================================

    [[nodiscard]] VkBuffer buffer() const noexcept { return m_buffer; }
    [[nodiscard]] VmaAllocation allocation() const noexcept { return m_allocation; }
    [[nodiscard]] VmaAllocationInfo allocation_info() const noexcept { return m_allocation_info; }

    /**
     * @brief Check if the buffer is persistently mapped
     */
    [[nodiscard]] bool is_persistently_mapped() const noexcept {
        return m_persistent_map != nullptr;
    }

    /**
     * @brief Get the persistently mapped pointer (for CPU-visible buffers)
     */
    [[nodiscard]] void* persistent_map() const noexcept { return m_persistent_map; }

private:
    VulkanDevice& m_device;

    VkBuffer m_buffer{VK_NULL_HANDLE};
    VmaAllocation m_allocation{VK_NULL_HANDLE};
    VmaAllocationInfo m_allocation_info{};

    u64 m_size{0};
    BufferUsage m_usage{BufferUsage::None};
    MemoryUsage m_memory_usage{MemoryUsage::GPU_Only};

    void* m_persistent_map{nullptr};
    bool m_is_mapped{false};
};

} // namespace hz::rhi::vk
