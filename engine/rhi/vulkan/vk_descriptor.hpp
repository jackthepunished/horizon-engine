#pragma once

/**
 * @file vk_descriptor.hpp
 * @brief Vulkan Descriptor Set Layout, Pool, and Set implementations
 *
 * Implements the RHI descriptor interfaces for Vulkan, managing:
 * - VkDescriptorSetLayout for binding layouts
 * - VkDescriptorPool for descriptor allocation
 * - VkDescriptorSet for actual resource bindings
 */

#include "../rhi_descriptor.hpp"
#include "vk_common.hpp"

namespace hz::rhi::vk {

class VulkanDevice;

// ============================================================================
// Vulkan Descriptor Set Layout
// ============================================================================

/**
 * @brief Vulkan implementation of the DescriptorSetLayout interface
 *
 * Wraps VkDescriptorSetLayout and stores binding information for queries.
 */
class VulkanDescriptorSetLayout final : public DescriptorSetLayout {
public:
    VulkanDescriptorSetLayout(VulkanDevice& device, const DescriptorSetLayoutDesc& desc);
    ~VulkanDescriptorSetLayout() override;

    // ========================================================================
    // DescriptorSetLayout Interface
    // ========================================================================

    [[nodiscard]] u32 binding_count() const noexcept override {
        return static_cast<u32>(m_bindings.size());
    }

    [[nodiscard]] const DescriptorBinding& binding(u32 index) const override {
        return m_bindings[index];
    }

    [[nodiscard]] u64 native_handle() const noexcept override {
        return reinterpret_cast<u64>(m_layout);
    }

    // ========================================================================
    // Vulkan-Specific Accessors
    // ========================================================================

    [[nodiscard]] VkDescriptorSetLayout layout() const noexcept { return m_layout; }

    /**
     * @brief Check if this layout uses push descriptors
     */
    [[nodiscard]] bool is_push_descriptor() const noexcept { return m_is_push_descriptor; }

    /**
     * @brief Get binding info by binding index (not array index)
     * @return Pointer to binding info, or nullptr if not found
     */
    [[nodiscard]] const DescriptorBinding* find_binding(u32 binding_index) const noexcept;

private:
    VulkanDevice& m_device;

    VkDescriptorSetLayout m_layout{VK_NULL_HANDLE};
    std::vector<DescriptorBinding> m_bindings;
    bool m_is_push_descriptor{false};
};

// ============================================================================
// Vulkan Descriptor Pool
// ============================================================================

/**
 * @brief Vulkan implementation of the DescriptorPool interface
 *
 * Manages descriptor set allocations from a VkDescriptorPool.
 */
class VulkanDescriptorPool final : public DescriptorPool {
public:
    VulkanDescriptorPool(VulkanDevice& device, const DescriptorPoolDesc& desc);
    ~VulkanDescriptorPool() override;

    // ========================================================================
    // DescriptorPool Interface
    // ========================================================================

    [[nodiscard]] std::unique_ptr<DescriptorSet>
    allocate(const DescriptorSetLayout& layout) override;

    void reset() override;

    [[nodiscard]] u64 native_handle() const noexcept override {
        return reinterpret_cast<u64>(m_pool);
    }

    // ========================================================================
    // Vulkan-Specific Accessors
    // ========================================================================

    [[nodiscard]] VkDescriptorPool pool() const noexcept { return m_pool; }

    /**
     * @brief Check if individual set freeing is enabled
     */
    [[nodiscard]] bool can_free_individual_sets() const noexcept { return m_free_individual_sets; }

private:
    VulkanDevice& m_device;

    VkDescriptorPool m_pool{VK_NULL_HANDLE};
    bool m_free_individual_sets{false};
};

// ============================================================================
// Vulkan Descriptor Set
// ============================================================================

/**
 * @brief Vulkan implementation of the DescriptorSet interface
 *
 * Wraps VkDescriptorSet and provides methods for updating bindings.
 */
class VulkanDescriptorSet final : public DescriptorSet {
public:
    /**
     * @brief Create a descriptor set from a pool allocation
     *
     * @param device The Vulkan device
     * @param pool The pool this set was allocated from (may be null for externally managed)
     * @param layout The layout this set conforms to
     * @param set The allocated VkDescriptorSet
     */
    VulkanDescriptorSet(VulkanDevice& device, VulkanDescriptorPool* pool,
                        const VulkanDescriptorSetLayout& layout, VkDescriptorSet set);
    ~VulkanDescriptorSet() override;

    // ========================================================================
    // DescriptorSet Interface
    // ========================================================================

    [[nodiscard]] const DescriptorSetLayout& layout() const noexcept override { return m_layout; }

    void write(std::span<const DescriptorWrite> writes) override;

    [[nodiscard]] u64 native_handle() const noexcept override {
        return reinterpret_cast<u64>(m_set);
    }

    // ========================================================================
    // Vulkan-Specific Accessors
    // ========================================================================

    [[nodiscard]] VkDescriptorSet set() const noexcept { return m_set; }

private:
    VulkanDevice& m_device;
    VulkanDescriptorPool* m_pool; // May be null if externally managed
    const VulkanDescriptorSetLayout& m_layout;
    VkDescriptorSet m_set{VK_NULL_HANDLE};
};

} // namespace hz::rhi::vk
