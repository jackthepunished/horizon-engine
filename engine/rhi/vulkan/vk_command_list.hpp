#pragma once

/**
 * @file vk_command_list.hpp
 * @brief Vulkan Command List (Command Buffer) implementation
 *
 * Implements the RHI CommandList interface for Vulkan, recording
 * commands into a VkCommandBuffer for later submission.
 */

#include "../rhi_command_list.hpp"
#include "vk_common.hpp"

namespace hz::rhi::vk {

class VulkanDevice;
class VulkanPipeline;

/**
 * @brief Vulkan implementation of the CommandList interface
 *
 * Wraps a VkCommandBuffer and provides methods for recording GPU commands.
 * The command buffer is allocated from the device's per-frame command pool.
 */
class VulkanCommandList final : public CommandList {
public:
    /**
     * @brief Create a command list for a specific queue type
     * @param device The Vulkan device
     * @param queue_type Type of queue this command list will be submitted to
     */
    VulkanCommandList(VulkanDevice& device, QueueType queue_type);
    ~VulkanCommandList() override;

    // ========================================================================
    // Recording Control
    // ========================================================================

    void begin() override;
    void end() override;
    void reset() override;

    [[nodiscard]] QueueType queue_type() const noexcept override { return m_queue_type; }

    // ========================================================================
    // Resource Barriers / Transitions
    // ========================================================================

    void barrier(const MemoryBarrier& barrier) override;
    void barrier(const BufferBarrier& barrier) override;
    void barrier(const TextureBarrier& barrier) override;
    void barriers(std::span<const BufferBarrier> buffer_barriers) override;
    void barriers(std::span<const TextureBarrier> texture_barriers) override;
    void barriers(std::span<const BufferBarrier> buffer_barriers,
                  std::span<const TextureBarrier> texture_barriers) override;

    // ========================================================================
    // Render Pass Commands
    // ========================================================================

    void begin_render_pass(const RenderPassBeginInfo& info) override;
    void end_render_pass() override;
    void next_subpass() override;

    // ========================================================================
    // Pipeline Binding
    // ========================================================================

    void bind_pipeline(const Pipeline& pipeline) override;

    // ========================================================================
    // Descriptor Set Binding
    // ========================================================================

    void bind_descriptor_sets(const PipelineLayout& layout, u32 first_set,
                              std::span<const DescriptorSet* const> sets,
                              std::span<const u32> dynamic_offsets = {}) override;

    // ========================================================================
    // Push Constants
    // ========================================================================

    void push_constants(const PipelineLayout& layout, ShaderStage stages, u32 offset, u32 size,
                        const void* data) override;

    // ========================================================================
    // Vertex/Index Buffer Binding
    // ========================================================================

    void bind_vertex_buffers(u32 first_binding, std::span<const Buffer* const> buffers,
                             std::span<const u64> offsets = {}) override;

    void bind_index_buffer(const Buffer& buffer, u64 offset = 0,
                           IndexType type = IndexType::Uint32) override;

    // ========================================================================
    // Dynamic State
    // ========================================================================

    void set_viewport(const Viewport& viewport) override;
    void set_viewports(u32 first_viewport, std::span<const Viewport> viewports) override;
    void set_scissor(const Scissor& scissor) override;
    void set_scissors(u32 first_scissor, std::span<const Scissor> scissors) override;
    void set_blend_constants(const f32 constants[4]) override;
    void set_depth_bias(f32 constant_factor, f32 clamp, f32 slope_factor) override;
    void set_stencil_reference(u32 reference) override;
    void set_line_width(f32 width) override;

    // ========================================================================
    // Draw Commands
    // ========================================================================

    void draw(u32 vertex_count, u32 instance_count = 1, u32 first_vertex = 0,
              u32 first_instance = 0) override;

    void draw_indexed(u32 index_count, u32 instance_count = 1, u32 first_index = 0,
                      i32 vertex_offset = 0, u32 first_instance = 0) override;

    void draw_indirect(const Buffer& buffer, u64 offset, u32 draw_count, u32 stride) override;

    void draw_indexed_indirect(const Buffer& buffer, u64 offset, u32 draw_count,
                               u32 stride) override;

    void draw_indirect_count(const Buffer& buffer, u64 offset, const Buffer& count_buffer,
                             u64 count_offset, u32 max_draw_count, u32 stride) override;

    void draw_indexed_indirect_count(const Buffer& buffer, u64 offset, const Buffer& count_buffer,
                                     u64 count_offset, u32 max_draw_count, u32 stride) override;

    // ========================================================================
    // Compute Commands
    // ========================================================================

    void dispatch(u32 group_count_x, u32 group_count_y = 1, u32 group_count_z = 1) override;
    void dispatch_indirect(const Buffer& buffer, u64 offset = 0) override;

    // ========================================================================
    // Copy Commands
    // ========================================================================

    void copy_buffer(const Buffer& src, Buffer& dst,
                     std::span<const BufferCopyRegion> regions) override;

    void copy_buffer_to_texture(const Buffer& src, Texture& dst,
                                std::span<const BufferTextureCopyRegion> regions) override;

    void copy_texture_to_buffer(const Texture& src, Buffer& dst,
                                std::span<const BufferTextureCopyRegion> regions) override;

    void copy_texture(const Texture& src, Texture& dst,
                      std::span<const TextureCopyRegion> regions) override;

    void blit_texture(const Texture& src, Texture& dst, const TextureCopyRegion& src_region,
                      const TextureCopyRegion& dst_region, Filter filter = Filter::Linear) override;

    // ========================================================================
    // Clear Commands
    // ========================================================================

    void clear_buffer(Buffer& buffer, u64 offset, u64 size, u32 value) override;

    void clear_texture(Texture& texture, const ClearColor& color, u32 base_mip_level = 0,
                       u32 mip_level_count = UINT32_MAX, u32 base_array_layer = 0,
                       u32 array_layer_count = UINT32_MAX) override;

    void clear_depth_stencil(Texture& texture, const ClearDepthStencil& value,
                             u32 base_mip_level = 0, u32 mip_level_count = UINT32_MAX,
                             u32 base_array_layer = 0, u32 array_layer_count = UINT32_MAX) override;

    // ========================================================================
    // Debug Markers
    // ========================================================================

    void begin_debug_marker(const char* name, const f32 color[4] = nullptr) override;
    void end_debug_marker() override;
    void insert_debug_marker(const char* name, const f32 color[4] = nullptr) override;

    // ========================================================================
    // Native Handle
    // ========================================================================

    [[nodiscard]] u64 native_handle() const noexcept override {
        return reinterpret_cast<u64>(m_command_buffer);
    }

    // ========================================================================
    // Vulkan-Specific Accessors
    // ========================================================================

    [[nodiscard]] VkCommandBuffer command_buffer() const noexcept { return m_command_buffer; }
    [[nodiscard]] VkCommandPool command_pool() const noexcept { return m_command_pool; }
    [[nodiscard]] bool is_recording() const noexcept { return m_is_recording; }
    [[nodiscard]] bool is_inside_render_pass() const noexcept { return m_inside_render_pass; }

private:
    VulkanDevice& m_device;

    VkCommandPool m_command_pool{VK_NULL_HANDLE};
    VkCommandBuffer m_command_buffer{VK_NULL_HANDLE};

    QueueType m_queue_type{QueueType::Graphics};
    bool m_is_recording{false};
    bool m_inside_render_pass{false};

    // Currently bound pipeline (for determining bind point)
    const VulkanPipeline* m_bound_pipeline{nullptr};

    // Helper to get pipeline bind point
    [[nodiscard]] VkPipelineBindPoint current_bind_point() const noexcept;
};

} // namespace hz::rhi::vk
