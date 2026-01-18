#pragma once

/**
 * @file rhi_descriptor.hpp
 * @brief RHI Descriptor Set Layout and Descriptor Set interfaces
 *
 * Descriptors are the mechanism for binding resources (buffers, textures, samplers)
 * to shader stages. This follows the Vulkan model of descriptor set layouts and
 * descriptor sets, which maps well to DX12 descriptor heaps and OpenGL bindings.
 */

#include "rhi_resources.hpp"
#include "rhi_types.hpp"

#include <vector>

namespace hz::rhi {

// ============================================================================
// Descriptor Binding Description
// ============================================================================

/**
 * @brief Describes a single binding within a descriptor set layout
 */
struct DescriptorBinding {
    u32 binding{0}; ///< Binding index in the shader
    DescriptorType type{DescriptorType::UniformBuffer};
    u32 count{1};                              ///< Number of descriptors (for arrays)
    ShaderStage stages{ShaderStage::All};      ///< Which stages can access this binding
    const Sampler* immutable_sampler{nullptr}; ///< Optional immutable/static sampler

    // ========================================================================
    // Factory Methods
    // ========================================================================

    /**
     * @brief Create a uniform buffer binding
     */
    [[nodiscard]] static DescriptorBinding uniform_buffer(u32 binding,
                                                          ShaderStage stages = ShaderStage::All) {
        return {binding, DescriptorType::UniformBuffer, 1, stages, nullptr};
    }

    /**
     * @brief Create a storage buffer binding
     */
    [[nodiscard]] static DescriptorBinding storage_buffer(u32 binding,
                                                          ShaderStage stages = ShaderStage::All) {
        return {binding, DescriptorType::StorageBuffer, 1, stages, nullptr};
    }

    /**
     * @brief Create a combined image sampler binding
     */
    [[nodiscard]] static DescriptorBinding
    combined_image_sampler(u32 binding, ShaderStage stages = ShaderStage::Fragment, u32 count = 1) {
        return {binding, DescriptorType::CombinedImageSampler, count, stages, nullptr};
    }

    /**
     * @brief Create a sampled image (texture without sampler) binding
     */
    [[nodiscard]] static DescriptorBinding
    sampled_image(u32 binding, ShaderStage stages = ShaderStage::Fragment, u32 count = 1) {
        return {binding, DescriptorType::SampledImage, count, stages, nullptr};
    }

    /**
     * @brief Create a storage image binding
     */
    [[nodiscard]] static DescriptorBinding
    storage_image(u32 binding, ShaderStage stages = ShaderStage::Compute, u32 count = 1) {
        return {binding, DescriptorType::StorageImage, count, stages, nullptr};
    }

    /**
     * @brief Create a sampler binding
     */
    [[nodiscard]] static DescriptorBinding sampler(u32 binding,
                                                   ShaderStage stages = ShaderStage::Fragment) {
        return {binding, DescriptorType::Sampler, 1, stages, nullptr};
    }

    /**
     * @brief Create an immutable sampler binding
     */
    [[nodiscard]] static DescriptorBinding
    immutable_sampler_binding(u32 binding, const Sampler* sampler,
                              ShaderStage stages = ShaderStage::Fragment) {
        return {binding, DescriptorType::Sampler, 1, stages, sampler};
    }
};

// ============================================================================
// Descriptor Set Layout
// ============================================================================

/**
 * @brief Description for creating a descriptor set layout
 */
struct DescriptorSetLayoutDesc {
    std::vector<DescriptorBinding> bindings;
    bool push_descriptor_set{false}; ///< Use push descriptors instead of pre-allocated sets
    const char* debug_name{nullptr};

    /**
     * @brief Create a layout for camera/view data (binding 0)
     */
    [[nodiscard]] static DescriptorSetLayoutDesc camera_layout() {
        DescriptorSetLayoutDesc desc;
        desc.bindings.push_back(
            DescriptorBinding::uniform_buffer(0, ShaderStage::Vertex | ShaderStage::Fragment));
        desc.debug_name = "CameraLayout";
        return desc;
    }

    /**
     * @brief Create a layout for scene/lighting data
     */
    [[nodiscard]] static DescriptorSetLayoutDesc scene_layout() {
        DescriptorSetLayoutDesc desc;
        desc.bindings.push_back(
            DescriptorBinding::uniform_buffer(0, ShaderStage::Fragment)); // Scene data
        desc.bindings.push_back(
            DescriptorBinding::combined_image_sampler(1, ShaderStage::Fragment)); // Shadow map
        desc.debug_name = "SceneLayout";
        return desc;
    }

    /**
     * @brief Create a layout for PBR material textures
     */
    [[nodiscard]] static DescriptorSetLayoutDesc material_layout() {
        DescriptorSetLayoutDesc desc;
        // Albedo, Normal, Metallic, Roughness, AO
        desc.bindings.push_back(
            DescriptorBinding::combined_image_sampler(0, ShaderStage::Fragment)); // Albedo
        desc.bindings.push_back(
            DescriptorBinding::combined_image_sampler(1, ShaderStage::Fragment)); // Normal
        desc.bindings.push_back(
            DescriptorBinding::combined_image_sampler(2, ShaderStage::Fragment)); // Metallic
        desc.bindings.push_back(
            DescriptorBinding::combined_image_sampler(3, ShaderStage::Fragment)); // Roughness
        desc.bindings.push_back(
            DescriptorBinding::combined_image_sampler(4, ShaderStage::Fragment)); // AO
        desc.debug_name = "MaterialLayout";
        return desc;
    }

    /**
     * @brief Create a layout for per-object data
     */
    [[nodiscard]] static DescriptorSetLayoutDesc object_layout() {
        DescriptorSetLayoutDesc desc;
        desc.bindings.push_back(
            DescriptorBinding::uniform_buffer(0, ShaderStage::Vertex)); // Model matrix, etc.
        desc.debug_name = "ObjectLayout";
        return desc;
    }
};

/**
 * @brief Abstract descriptor set layout interface
 *
 * Defines the structure of a descriptor set - what types of resources
 * are bound at what binding points.
 */
class DescriptorSetLayout {
public:
    virtual ~DescriptorSetLayout() = default;

    /**
     * @brief Get the number of bindings in this layout
     */
    [[nodiscard]] virtual u32 binding_count() const noexcept = 0;

    /**
     * @brief Get binding information
     */
    [[nodiscard]] virtual const DescriptorBinding& binding(u32 index) const = 0;

    [[nodiscard]] virtual u64 native_handle() const noexcept = 0;

protected:
    DescriptorSetLayout() = default;
    HZ_NON_COPYABLE(DescriptorSetLayout);
    HZ_NON_MOVABLE(DescriptorSetLayout);
};

// ============================================================================
// Descriptor Write Information
// ============================================================================

/**
 * @brief Buffer binding information for descriptor writes
 */
struct DescriptorBufferInfo {
    const Buffer* buffer{nullptr};
    u64 offset{0};
    u64 range{UINT64_MAX}; ///< UINT64_MAX = entire remaining buffer

    DescriptorBufferInfo() = default;
    DescriptorBufferInfo(const Buffer* buf, u64 off = 0, u64 rng = UINT64_MAX)
        : buffer(buf), offset(off), range(rng) {}
};

/**
 * @brief Image/texture binding information for descriptor writes
 */
struct DescriptorImageInfo {
    const Sampler* sampler{nullptr};
    const TextureView* texture_view{nullptr};
    ResourceState layout{ResourceState::ShaderResource};

    DescriptorImageInfo() = default;
    DescriptorImageInfo(const TextureView* view, const Sampler* samp = nullptr,
                        ResourceState state = ResourceState::ShaderResource)
        : sampler(samp), texture_view(view), layout(state) {}
};

/**
 * @brief Describes an update to a descriptor set binding
 */
struct DescriptorWrite {
    u32 binding{0};
    u32 array_element{0}; ///< Starting array element for arrays
    DescriptorType type{DescriptorType::UniformBuffer};

    // One of these vectors should be populated based on type
    std::vector<DescriptorBufferInfo> buffer_infos;
    std::vector<DescriptorImageInfo> image_infos;

    // ========================================================================
    // Factory Methods
    // ========================================================================

    /**
     * @brief Create a write for a uniform buffer
     */
    [[nodiscard]] static DescriptorWrite uniform_buffer(u32 binding, const Buffer& buffer,
                                                        u64 offset = 0, u64 range = UINT64_MAX) {
        DescriptorWrite write;
        write.binding = binding;
        write.type = DescriptorType::UniformBuffer;
        write.buffer_infos.push_back({&buffer, offset, range});
        return write;
    }

    /**
     * @brief Create a write for a storage buffer
     */
    [[nodiscard]] static DescriptorWrite storage_buffer(u32 binding, const Buffer& buffer,
                                                        u64 offset = 0, u64 range = UINT64_MAX) {
        DescriptorWrite write;
        write.binding = binding;
        write.type = DescriptorType::StorageBuffer;
        write.buffer_infos.push_back({&buffer, offset, range});
        return write;
    }

    /**
     * @brief Create a write for a combined image sampler
     */
    [[nodiscard]] static DescriptorWrite
    combined_image_sampler(u32 binding, const TextureView& view, const Sampler& sampler) {
        DescriptorWrite write;
        write.binding = binding;
        write.type = DescriptorType::CombinedImageSampler;
        write.image_infos.push_back({&view, &sampler, ResourceState::ShaderResource});
        return write;
    }

    /**
     * @brief Create a write for a sampled image
     */
    [[nodiscard]] static DescriptorWrite sampled_image(u32 binding, const TextureView& view) {
        DescriptorWrite write;
        write.binding = binding;
        write.type = DescriptorType::SampledImage;
        write.image_infos.push_back({&view, nullptr, ResourceState::ShaderResource});
        return write;
    }

    /**
     * @brief Create a write for a storage image
     */
    [[nodiscard]] static DescriptorWrite storage_image(u32 binding, const TextureView& view) {
        DescriptorWrite write;
        write.binding = binding;
        write.type = DescriptorType::StorageImage;
        write.image_infos.push_back({&view, nullptr, ResourceState::UnorderedAccess});
        return write;
    }

    /**
     * @brief Create a write for a sampler
     */
    [[nodiscard]] static DescriptorWrite sampler(u32 binding, const Sampler& samp) {
        DescriptorWrite write;
        write.binding = binding;
        write.type = DescriptorType::Sampler;
        write.image_infos.push_back({nullptr, &samp, ResourceState::Undefined});
        return write;
    }
};

// ============================================================================
// Descriptor Set
// ============================================================================

/**
 * @brief Abstract descriptor set interface
 *
 * A descriptor set is an allocated instance of a descriptor set layout,
 * with actual resources bound to it.
 */
class DescriptorSet {
public:
    virtual ~DescriptorSet() = default;

    /**
     * @brief Get the layout this set was created from
     */
    [[nodiscard]] virtual const DescriptorSetLayout& layout() const noexcept = 0;

    /**
     * @brief Update multiple bindings at once
     * @param writes Array of descriptor writes
     */
    virtual void write(std::span<const DescriptorWrite> writes) = 0;

    // ========================================================================
    // Convenience Write Methods
    // ========================================================================

    /**
     * @brief Bind a uniform buffer to a binding
     */
    void write_buffer(u32 binding, const Buffer& buffer, u64 offset = 0, u64 range = UINT64_MAX) {
        DescriptorWrite w = DescriptorWrite::uniform_buffer(binding, buffer, offset, range);
        write({&w, 1});
    }

    /**
     * @brief Bind a storage buffer to a binding
     */
    void write_storage_buffer(u32 binding, const Buffer& buffer, u64 offset = 0,
                              u64 range = UINT64_MAX) {
        DescriptorWrite w = DescriptorWrite::storage_buffer(binding, buffer, offset, range);
        write({&w, 1});
    }

    /**
     * @brief Bind a combined image sampler to a binding
     */
    void write_texture(u32 binding, const TextureView& view, const Sampler& sampler) {
        DescriptorWrite w = DescriptorWrite::combined_image_sampler(binding, view, sampler);
        write({&w, 1});
    }

    /**
     * @brief Bind a sampled image (texture without sampler) to a binding
     */
    void write_image(u32 binding, const TextureView& view) {
        DescriptorWrite w = DescriptorWrite::sampled_image(binding, view);
        write({&w, 1});
    }

    /**
     * @brief Bind a storage image to a binding
     */
    void write_storage_image(u32 binding, const TextureView& view) {
        DescriptorWrite w = DescriptorWrite::storage_image(binding, view);
        write({&w, 1});
    }

    /**
     * @brief Bind a sampler to a binding
     */
    void write_sampler(u32 binding, const Sampler& sampler) {
        DescriptorWrite w = DescriptorWrite::sampler(binding, sampler);
        write({&w, 1});
    }

    [[nodiscard]] virtual u64 native_handle() const noexcept = 0;

protected:
    DescriptorSet() = default;
    HZ_NON_COPYABLE(DescriptorSet);
    HZ_NON_MOVABLE(DescriptorSet);
};

// ============================================================================
// Descriptor Pool (for allocation)
// ============================================================================

/**
 * @brief Pool size for a specific descriptor type
 */
struct DescriptorPoolSize {
    DescriptorType type{DescriptorType::UniformBuffer};
    u32 count{0};
};

/**
 * @brief Description for creating a descriptor pool
 */
struct DescriptorPoolDesc {
    std::vector<DescriptorPoolSize> pool_sizes;
    u32 max_sets{0};
    bool free_individual_sets{false}; ///< Allow freeing individual sets
    const char* debug_name{nullptr};
};

/**
 * @brief Abstract descriptor pool interface
 *
 * Descriptor pools manage the memory for descriptor sets.
 */
class DescriptorPool {
public:
    virtual ~DescriptorPool() = default;

    /**
     * @brief Allocate a descriptor set from this pool
     */
    [[nodiscard]] virtual std::unique_ptr<DescriptorSet>
    allocate(const DescriptorSetLayout& layout) = 0;

    /**
     * @brief Reset the pool, freeing all allocated sets
     */
    virtual void reset() = 0;

    [[nodiscard]] virtual u64 native_handle() const noexcept = 0;

protected:
    DescriptorPool() = default;
    HZ_NON_COPYABLE(DescriptorPool);
    HZ_NON_MOVABLE(DescriptorPool);
};

} // namespace hz::rhi
