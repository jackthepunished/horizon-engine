#include "vk_buffer.hpp"

#include "vk_device.hpp"

namespace hz::rhi::vk {

VulkanBuffer::VulkanBuffer(VulkanDevice& device, const BufferDesc& desc)
    : m_device(device), m_size(desc.size), m_usage(desc.usage), m_memory_usage(desc.memory) {

    // Create buffer
    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = m_size;
    buffer_info.usage = to_vk_buffer_usage(m_usage);
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    // Always allow transfer operations for convenience
    if (!(buffer_info.usage & VK_BUFFER_USAGE_TRANSFER_SRC_BIT)) {
        buffer_info.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    }

    // VMA allocation info
    VmaAllocationCreateInfo alloc_info{};
    alloc_info.usage = to_vma_memory_usage(m_memory_usage);

    // For CPU-visible memory, use persistent mapping
    if (m_memory_usage == MemoryUsage::CPU_To_GPU || m_memory_usage == MemoryUsage::GPU_To_CPU ||
        m_memory_usage == MemoryUsage::CPU_Only) {
        alloc_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT |
                           VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    }

    // Create buffer with VMA
    VkResult result = vmaCreateBuffer(m_device.allocator(), &buffer_info, &alloc_info, &m_buffer,
                                      &m_allocation, &m_allocation_info);

    if (result != VK_SUCCESS) {
        HZ_LOG_ERROR("Failed to create Vulkan buffer: {}", vk_result_string(result));
        return;
    }

    // Store persistent map pointer if allocated with MAPPED flag
    if (m_allocation_info.pMappedData) {
        m_persistent_map = m_allocation_info.pMappedData;
    }

    // Set debug name
    if (desc.debug_name) {
        m_device.set_debug_name(reinterpret_cast<u64>(m_buffer), desc.debug_name);
    }

    // Upload initial data if provided
    if (desc.initial_data && m_size > 0) {
        if (m_persistent_map) {
            // Direct copy for CPU-visible memory
            std::memcpy(m_persistent_map, desc.initial_data, m_size);
            flush();
        } else {
            // Need staging buffer for GPU-only memory
            // This is handled by Device::update_buffer() for now
            HZ_LOG_WARN("Initial data upload for GPU-only buffer requires staging buffer");
        }
    }

    HZ_LOG_DEBUG("Created Vulkan buffer: size={}, usage={:#x}", m_size, static_cast<u32>(m_usage));
}

VulkanBuffer::~VulkanBuffer() {
    if (m_buffer != VK_NULL_HANDLE) {
        // Unmap if still mapped
        if (m_is_mapped && !m_persistent_map) {
            vmaUnmapMemory(m_device.allocator(), m_allocation);
        }

        // Defer deletion to avoid destroying resources in use
        m_device.defer_deletion(m_buffer, m_allocation);
        m_buffer = VK_NULL_HANDLE;
        m_allocation = VK_NULL_HANDLE;
    }
}

void* VulkanBuffer::map() {
    // Return persistent map if available
    if (m_persistent_map) {
        return m_persistent_map;
    }

    // Already mapped?
    if (m_is_mapped) {
        HZ_LOG_WARN("Buffer already mapped");
        return nullptr;
    }

    // Can't map GPU-only memory
    if (m_memory_usage == MemoryUsage::GPU_Only) {
        HZ_LOG_ERROR("Cannot map GPU-only buffer");
        return nullptr;
    }

    void* data = nullptr;
    VkResult result = vmaMapMemory(m_device.allocator(), m_allocation, &data);
    if (result != VK_SUCCESS) {
        HZ_LOG_ERROR("Failed to map buffer: {}", vk_result_string(result));
        return nullptr;
    }

    m_is_mapped = true;
    return data;
}

void VulkanBuffer::unmap() {
    // Persistent maps don't need unmapping
    if (m_persistent_map) {
        return;
    }

    if (!m_is_mapped) {
        HZ_LOG_WARN("Buffer not mapped");
        return;
    }

    vmaUnmapMemory(m_device.allocator(), m_allocation);
    m_is_mapped = false;
}

void VulkanBuffer::flush(u64 offset, u64 size) {
    if (m_memory_usage == MemoryUsage::GPU_Only) {
        return;
    }

    if (size == UINT64_MAX) {
        size = m_size - offset;
    }

    vmaFlushAllocation(m_device.allocator(), m_allocation, offset, size);
}

void VulkanBuffer::invalidate(u64 offset, u64 size) {
    if (m_memory_usage == MemoryUsage::GPU_Only) {
        return;
    }

    if (size == UINT64_MAX) {
        size = m_size - offset;
    }

    vmaInvalidateAllocation(m_device.allocator(), m_allocation, offset, size);
}

} // namespace hz::rhi::vk
