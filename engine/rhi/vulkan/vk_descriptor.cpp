/**
 * @file vk_descriptor.cpp
 * @brief Vulkan Descriptor Set Layout, Pool, and Set implementations
 */

#include "vk_descriptor.hpp"

#include "vk_buffer.hpp"
#include "vk_device.hpp"
#include "vk_sampler.hpp"
#include "vk_texture.hpp"

namespace hz::rhi::vk {

// ============================================================================
// VulkanDescriptorSetLayout Implementation
// ============================================================================

VulkanDescriptorSetLayout::VulkanDescriptorSetLayout(VulkanDevice& device,
                                                     const DescriptorSetLayoutDesc& desc)
    : m_device(device), m_bindings(desc.bindings), m_is_push_descriptor(desc.push_descriptor_set) {

    // Build Vulkan binding array
    std::vector<VkDescriptorSetLayoutBinding> vk_bindings;
    vk_bindings.reserve(desc.bindings.size());

    // Storage for immutable samplers (must outlive vkCreateDescriptorSetLayout call)
    std::vector<VkSampler> immutable_samplers;

    for (const auto& binding : desc.bindings) {
        VkDescriptorSetLayoutBinding vk_binding{};
        vk_binding.binding = binding.binding;
        vk_binding.descriptorType = to_vk_descriptor_type(binding.type);
        vk_binding.descriptorCount = binding.count;
        vk_binding.stageFlags = to_vk_shader_stages(binding.stages);
        vk_binding.pImmutableSamplers = nullptr;

        // Handle immutable sampler
        if (binding.immutable_sampler != nullptr) {
            auto* vk_sampler = static_cast<const VulkanSampler*>(binding.immutable_sampler);
            immutable_samplers.push_back(vk_sampler->sampler());
            vk_binding.pImmutableSamplers = &immutable_samplers.back();
        }

        vk_bindings.push_back(vk_binding);
    }

    // Create the layout
    VkDescriptorSetLayoutCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    create_info.bindingCount = static_cast<u32>(vk_bindings.size());
    create_info.pBindings = vk_bindings.data();

    // Set push descriptor flag if requested
    if (m_is_push_descriptor) {
        create_info.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
    }

    VK_CHECK_FATAL(
        vkCreateDescriptorSetLayout(m_device.device(), &create_info, nullptr, &m_layout));

    // Set debug name if provided
    if (desc.debug_name) {
        m_device.set_debug_name(reinterpret_cast<u64>(m_layout), desc.debug_name);
    }

    HZ_LOG_DEBUG("Created VkDescriptorSetLayout with {} bindings", vk_bindings.size());
}

VulkanDescriptorSetLayout::~VulkanDescriptorSetLayout() {
    if (m_layout != VK_NULL_HANDLE) {
        m_device.defer_deletion(m_layout);
        m_layout = VK_NULL_HANDLE;
    }
}

const DescriptorBinding* VulkanDescriptorSetLayout::find_binding(u32 binding_index) const noexcept {
    for (const auto& binding : m_bindings) {
        if (binding.binding == binding_index) {
            return &binding;
        }
    }
    return nullptr;
}

// ============================================================================
// VulkanDescriptorPool Implementation
// ============================================================================

VulkanDescriptorPool::VulkanDescriptorPool(VulkanDevice& device, const DescriptorPoolDesc& desc)
    : m_device(device), m_free_individual_sets(desc.free_individual_sets) {

    // Convert pool sizes
    std::vector<VkDescriptorPoolSize> vk_pool_sizes;
    vk_pool_sizes.reserve(desc.pool_sizes.size());

    for (const auto& size : desc.pool_sizes) {
        VkDescriptorPoolSize vk_size{};
        vk_size.type = to_vk_descriptor_type(size.type);
        vk_size.descriptorCount = size.count;
        vk_pool_sizes.push_back(vk_size);
    }

    // Create the pool
    VkDescriptorPoolCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    create_info.poolSizeCount = static_cast<u32>(vk_pool_sizes.size());
    create_info.pPoolSizes = vk_pool_sizes.data();
    create_info.maxSets = desc.max_sets;

    if (m_free_individual_sets) {
        create_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    }

    VK_CHECK_FATAL(vkCreateDescriptorPool(m_device.device(), &create_info, nullptr, &m_pool));

    // Set debug name if provided
    if (desc.debug_name) {
        m_device.set_debug_name(reinterpret_cast<u64>(m_pool), desc.debug_name);
    }

    HZ_LOG_DEBUG("Created VkDescriptorPool with {} pool sizes, max {} sets", vk_pool_sizes.size(),
                 desc.max_sets);
}

VulkanDescriptorPool::~VulkanDescriptorPool() {
    if (m_pool != VK_NULL_HANDLE) {
        m_device.defer_deletion(m_pool);
        m_pool = VK_NULL_HANDLE;
    }
}

std::unique_ptr<DescriptorSet> VulkanDescriptorPool::allocate(const DescriptorSetLayout& layout) {
    auto* vk_layout = static_cast<const VulkanDescriptorSetLayout*>(&layout);

    VkDescriptorSetAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = m_pool;
    alloc_info.descriptorSetCount = 1;
    VkDescriptorSetLayout vk_set_layout = vk_layout->layout();
    alloc_info.pSetLayouts = &vk_set_layout;

    VkDescriptorSet set{VK_NULL_HANDLE};
    VkResult result = vkAllocateDescriptorSets(m_device.device(), &alloc_info, &set);

    if (result != VK_SUCCESS) {
        HZ_LOG_ERROR("Failed to allocate descriptor set: {}", vk_result_string(result));
        return nullptr;
    }

    return std::make_unique<VulkanDescriptorSet>(m_device, this, *vk_layout, set);
}

void VulkanDescriptorPool::reset() {
    VK_CHECK(vkResetDescriptorPool(m_device.device(), m_pool, 0));
}

// ============================================================================
// VulkanDescriptorSet Implementation
// ============================================================================

VulkanDescriptorSet::VulkanDescriptorSet(VulkanDevice& device, VulkanDescriptorPool* pool,
                                         const VulkanDescriptorSetLayout& layout,
                                         VkDescriptorSet set)
    : m_device(device), m_pool(pool), m_layout(layout), m_set(set) {}

VulkanDescriptorSet::~VulkanDescriptorSet() {
    // Descriptor sets are implicitly freed when the pool is destroyed or reset
    // Only explicitly free if the pool supports individual freeing
    if (m_pool && m_pool->can_free_individual_sets() && m_set != VK_NULL_HANDLE) {
        vkFreeDescriptorSets(m_device.device(), m_pool->pool(), 1, &m_set);
    }
    m_set = VK_NULL_HANDLE;
}

void VulkanDescriptorSet::write(std::span<const DescriptorWrite> writes) {
    if (writes.empty()) {
        return;
    }

    // Temporary storage for Vulkan structures
    std::vector<VkWriteDescriptorSet> vk_writes;
    vk_writes.reserve(writes.size());

    // Storage for buffer and image infos (must outlive vkUpdateDescriptorSets call)
    std::vector<std::vector<VkDescriptorBufferInfo>> all_buffer_infos;
    std::vector<std::vector<VkDescriptorImageInfo>> all_image_infos;
    all_buffer_infos.reserve(writes.size());
    all_image_infos.reserve(writes.size());

    for (const auto& write : writes) {
        VkWriteDescriptorSet vk_write{};
        vk_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        vk_write.dstSet = m_set;
        vk_write.dstBinding = write.binding;
        vk_write.dstArrayElement = write.array_element;
        vk_write.descriptorType = to_vk_descriptor_type(write.type);

        // Handle buffer descriptors
        if (!write.buffer_infos.empty()) {
            all_buffer_infos.emplace_back();
            auto& buffer_infos = all_buffer_infos.back();
            buffer_infos.reserve(write.buffer_infos.size());

            for (const auto& info : write.buffer_infos) {
                VkDescriptorBufferInfo vk_info{};
                if (info.buffer) {
                    auto* vk_buffer = static_cast<const VulkanBuffer*>(info.buffer);
                    vk_info.buffer = vk_buffer->buffer();
                    vk_info.offset = info.offset;
                    vk_info.range = (info.range == UINT64_MAX) ? VK_WHOLE_SIZE : info.range;
                }
                buffer_infos.push_back(vk_info);
            }

            vk_write.descriptorCount = static_cast<u32>(buffer_infos.size());
            vk_write.pBufferInfo = buffer_infos.data();
        }
        // Handle image/sampler descriptors
        else if (!write.image_infos.empty()) {
            all_image_infos.emplace_back();
            auto& image_infos = all_image_infos.back();
            image_infos.reserve(write.image_infos.size());

            for (const auto& info : write.image_infos) {
                VkDescriptorImageInfo vk_info{};

                if (info.sampler) {
                    auto* vk_sampler = static_cast<const VulkanSampler*>(info.sampler);
                    vk_info.sampler = vk_sampler->sampler();
                }

                if (info.texture_view) {
                    auto* vk_view = static_cast<const VulkanTextureView*>(info.texture_view);
                    vk_info.imageView = vk_view->image_view();
                    vk_info.imageLayout = to_vk_image_layout(info.layout);
                }

                image_infos.push_back(vk_info);
            }

            vk_write.descriptorCount = static_cast<u32>(image_infos.size());
            vk_write.pImageInfo = image_infos.data();
        }

        vk_writes.push_back(vk_write);
    }

    // Perform the update
    vkUpdateDescriptorSets(m_device.device(), static_cast<u32>(vk_writes.size()), vk_writes.data(),
                           0, nullptr);
}

} // namespace hz::rhi::vk
