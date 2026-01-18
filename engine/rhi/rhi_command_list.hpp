#pragma once

/**
 * @file rhi_command_list.hpp
 * @brief RHI Command List (Command Buffer) interface
 *
 * Command lists record GPU commands for later submission to a queue.
 * This follows the modern explicit API model where commands are recorded
 * into command buffers and then submitted as a batch.
 */

#include "rhi_descriptor.hpp"
#include "rhi_pipeline.hpp"
#include "rhi_resources.hpp"
#include "rhi_types.hpp"

#include <span>

namespace hz::rhi {

// ============================================================================
// Buffer Copy Regions
// ============================================================================

/**
 * @brief Describes a region for buffer-to-buffer copies
 */
struct BufferCopyRegion {
    u64 src_offset{0};
    u64 dst_offset{0};
    u64 size{0}; ///< 0 = copy entire source buffer
};

/**
 * @brief Describes a region for buffer-to-texture copies
 */
struct BufferTextureCopyRegion {
    u64 buffer_offset{0};
    u32 buffer_row_length{0};   ///< 0 = tightly packed
    u32 buffer_image_height{0}; ///< 0 = tightly packed
    u32 mip_level{0};
    u32 base_array_layer{0};
    u32 layer_count{1};
    Offset3D texture_offset{};
    Extent3D texture_extent{};
};

/**
 * @brief Describes a region for texture-to-texture copies
 */
struct TextureCopyRegion {
    u32 src_mip_level{0};
    u32 src_base_array_layer{0};
    u32 src_layer_count{1};
    Offset3D src_offset{};
    u32 dst_mip_level{0};
    u32 dst_base_array_layer{0};
    u32 dst_layer_count{1};
    Offset3D dst_offset{};
    Extent3D extent{};
};

// ============================================================================
// Resource Barriers
// ============================================================================

/**
 * @brief Memory barrier (global synchronization)
 */
struct MemoryBarrier {
    ShaderStage src_stages{ShaderStage::All};
    ShaderStage dst_stages{ShaderStage::All};
};

/**
 * @brief Buffer memory barrier
 */
struct BufferBarrier {
    const Buffer* buffer{nullptr};
    ResourceState old_state{ResourceState::Undefined};
    ResourceState new_state{ResourceState::Common};
    u64 offset{0};
    u64 size{UINT64_MAX}; ///< UINT64_MAX = entire buffer
};

/**
 * @brief Texture/image memory barrier
 */
struct TextureBarrier {
    const Texture* texture{nullptr};
    ResourceState old_state{ResourceState::Undefined};
    ResourceState new_state{ResourceState::Common};
    u32 base_mip_level{0};
    u32 mip_level_count{UINT32_MAX}; ///< UINT32_MAX = remaining mips
    u32 base_array_layer{0};
    u32 array_layer_count{UINT32_MAX}; ///< UINT32_MAX = remaining layers
};

// ============================================================================
// Render Pass Begin Info
// ============================================================================

/**
 * @brief Information for beginning a render pass
 */
struct RenderPassBeginInfo {
    const Framebuffer* framebuffer{nullptr};
    std::span<const ClearValue> clear_values{};
    Scissor render_area{}; ///< Render area (0,0 = use framebuffer size)
};

// ============================================================================
// Draw/Dispatch Arguments
// ============================================================================

/**
 * @brief Arguments for indirect draw commands
 */
struct DrawIndirectCommand {
    u32 vertex_count{0};
    u32 instance_count{1};
    u32 first_vertex{0};
    u32 first_instance{0};
};

/**
 * @brief Arguments for indexed indirect draw commands
 */
struct DrawIndexedIndirectCommand {
    u32 index_count{0};
    u32 instance_count{1};
    u32 first_index{0};
    i32 vertex_offset{0};
    u32 first_instance{0};
};

/**
 * @brief Arguments for indirect dispatch commands
 */
struct DispatchIndirectCommand {
    u32 group_count_x{1};
    u32 group_count_y{1};
    u32 group_count_z{1};
};

// ============================================================================
// Command List Interface
// ============================================================================

/**
 * @brief Abstract command list (command buffer) interface
 *
 * Records GPU commands for later submission. Command lists are not thread-safe -
 * each recording thread should have its own command list.
 *
 * Typical usage:
 * @code
 * cmd->begin();
 * cmd->begin_render_pass(framebuffer, clear_values);
 * cmd->bind_pipeline(pipeline);
 * cmd->bind_descriptor_sets(...);
 * cmd->bind_vertex_buffers(...);
 * cmd->draw(vertex_count, instance_count);
 * cmd->end_render_pass();
 * cmd->end();
 * device->submit(cmd);
 * @endcode
 */
class CommandList {
public:
    virtual ~CommandList() = default;

    // ========================================================================
    // Recording Control
    // ========================================================================

    /**
     * @brief Begin recording commands
     * @note Must be called before any command recording
     */
    virtual void begin() = 0;

    /**
     * @brief End recording commands
     * @note Must be called after all commands are recorded
     */
    virtual void end() = 0;

    /**
     * @brief Reset the command list for reuse
     * @note Command list must not be in flight on GPU
     */
    virtual void reset() = 0;

    /**
     * @brief Get the queue type this command list was created for
     */
    [[nodiscard]] virtual QueueType queue_type() const noexcept = 0;

    // ========================================================================
    // Resource Barriers / Transitions
    // ========================================================================

    /**
     * @brief Insert a memory barrier
     */
    virtual void barrier(const MemoryBarrier& barrier) = 0;

    /**
     * @brief Transition buffer resource state
     */
    virtual void barrier(const BufferBarrier& barrier) = 0;

    /**
     * @brief Transition texture resource state
     */
    virtual void barrier(const TextureBarrier& barrier) = 0;

    /**
     * @brief Batch multiple buffer barriers
     */
    virtual void barriers(std::span<const BufferBarrier> buffer_barriers) = 0;

    /**
     * @brief Batch multiple texture barriers
     */
    virtual void barriers(std::span<const TextureBarrier> texture_barriers) = 0;

    /**
     * @brief Batch both buffer and texture barriers
     */
    virtual void barriers(std::span<const BufferBarrier> buffer_barriers,
                          std::span<const TextureBarrier> texture_barriers) = 0;

    // ========================================================================
    // Render Pass Commands
    // ========================================================================

    /**
     * @brief Begin a render pass
     * @param info Render pass configuration including framebuffer and clear values
     */
    virtual void begin_render_pass(const RenderPassBeginInfo& info) = 0;

    /**
     * @brief Convenience overload for beginning a render pass
     */
    void begin_render_pass(const Framebuffer& fb, std::span<const ClearValue> clear_values = {}) {
        RenderPassBeginInfo info;
        info.framebuffer = &fb;
        info.clear_values = clear_values;
        begin_render_pass(info);
    }

    /**
     * @brief End the current render pass
     */
    virtual void end_render_pass() = 0;

    /**
     * @brief Advance to the next subpass (if using multi-subpass render pass)
     */
    virtual void next_subpass() = 0;

    // ========================================================================
    // Pipeline Binding
    // ========================================================================

    /**
     * @brief Bind a graphics or compute pipeline
     */
    virtual void bind_pipeline(const Pipeline& pipeline) = 0;

    // ========================================================================
    // Descriptor Set Binding
    // ========================================================================

    /**
     * @brief Bind descriptor sets
     * @param layout Pipeline layout
     * @param first_set Index of first set to bind
     * @param sets Descriptor sets to bind
     * @param dynamic_offsets Dynamic buffer offsets (for dynamic UBOs/SSBOs)
     */
    virtual void bind_descriptor_sets(const PipelineLayout& layout, u32 first_set,
                                      std::span<const DescriptorSet* const> sets,
                                      std::span<const u32> dynamic_offsets = {}) = 0;

    /**
     * @brief Convenience: bind a single descriptor set
     */
    void bind_descriptor_set(const PipelineLayout& layout, u32 set_index, const DescriptorSet& set,
                             std::span<const u32> dynamic_offsets = {}) {
        const DescriptorSet* ptr = &set;
        bind_descriptor_sets(layout, set_index, {&ptr, 1}, dynamic_offsets);
    }

    // ========================================================================
    // Push Constants
    // ========================================================================

    /**
     * @brief Update push constant data
     * @param layout Pipeline layout defining push constant ranges
     * @param stages Shader stages to update
     * @param offset Byte offset into push constant block
     * @param size Size of data in bytes
     * @param data Pointer to data
     */
    virtual void push_constants(const PipelineLayout& layout, ShaderStage stages, u32 offset,
                                u32 size, const void* data) = 0;

    /**
     * @brief Convenience: push a typed value
     */
    template <typename T>
    void push_constants(const PipelineLayout& layout, ShaderStage stages, const T& value,
                        u32 offset = 0) {
        push_constants(layout, stages, offset, sizeof(T), &value);
    }

    // ========================================================================
    // Vertex/Index Buffer Binding
    // ========================================================================

    /**
     * @brief Bind vertex buffers
     * @param first_binding First binding slot
     * @param buffers Vertex buffers to bind
     * @param offsets Byte offsets into each buffer
     */
    virtual void bind_vertex_buffers(u32 first_binding, std::span<const Buffer* const> buffers,
                                     std::span<const u64> offsets = {}) = 0;

    /**
     * @brief Convenience: bind a single vertex buffer
     */
    void bind_vertex_buffer(u32 binding, const Buffer& buffer, u64 offset = 0) {
        const Buffer* ptr = &buffer;
        u64 off = offset;
        bind_vertex_buffers(binding, {&ptr, 1}, {&off, 1});
    }

    /**
     * @brief Bind an index buffer
     * @param buffer Index buffer
     * @param offset Byte offset into buffer
     * @param type Index element type (16 or 32 bit)
     */
    virtual void bind_index_buffer(const Buffer& buffer, u64 offset = 0,
                                   IndexType type = IndexType::Uint32) = 0;

    // ========================================================================
    // Dynamic State
    // ========================================================================

    /**
     * @brief Set viewport(s) dynamically
     */
    virtual void set_viewport(const Viewport& viewport) = 0;

    /**
     * @brief Set multiple viewports
     */
    virtual void set_viewports(u32 first_viewport, std::span<const Viewport> viewports) = 0;

    /**
     * @brief Set scissor rect(s) dynamically
     */
    virtual void set_scissor(const Scissor& scissor) = 0;

    /**
     * @brief Set multiple scissor rects
     */
    virtual void set_scissors(u32 first_scissor, std::span<const Scissor> scissors) = 0;

    /**
     * @brief Set viewport and scissor to match an extent
     */
    void set_viewport_and_scissor(const Extent2D& extent) {
        set_viewport(Viewport{0.0f, 0.0f, static_cast<f32>(extent.width),
                              static_cast<f32>(extent.height), 0.0f, 1.0f});
        set_scissor(Scissor{0, 0, extent.width, extent.height});
    }

    /**
     * @brief Set blend constants
     */
    virtual void set_blend_constants(const f32 constants[4]) = 0;

    /**
     * @brief Set depth bias
     */
    virtual void set_depth_bias(f32 constant_factor, f32 clamp, f32 slope_factor) = 0;

    /**
     * @brief Set stencil reference value
     */
    virtual void set_stencil_reference(u32 reference) = 0;

    /**
     * @brief Set line width (if supported)
     */
    virtual void set_line_width(f32 width) = 0;

    // ========================================================================
    // Draw Commands
    // ========================================================================

    /**
     * @brief Draw non-indexed primitives
     * @param vertex_count Number of vertices to draw
     * @param instance_count Number of instances
     * @param first_vertex Offset to first vertex
     * @param first_instance Offset to first instance
     */
    virtual void draw(u32 vertex_count, u32 instance_count = 1, u32 first_vertex = 0,
                      u32 first_instance = 0) = 0;

    /**
     * @brief Draw indexed primitives
     * @param index_count Number of indices to draw
     * @param instance_count Number of instances
     * @param first_index Offset into index buffer
     * @param vertex_offset Value added to vertex index before indexing into vertex buffer
     * @param first_instance Offset to first instance
     */
    virtual void draw_indexed(u32 index_count, u32 instance_count = 1, u32 first_index = 0,
                              i32 vertex_offset = 0, u32 first_instance = 0) = 0;

    /**
     * @brief Draw non-indexed primitives with parameters from a buffer
     * @param buffer Buffer containing DrawIndirectCommand structures
     * @param offset Byte offset into buffer
     * @param draw_count Number of draws
     * @param stride Bytes between draw commands in buffer
     */
    virtual void draw_indirect(const Buffer& buffer, u64 offset, u32 draw_count, u32 stride) = 0;

    /**
     * @brief Draw indexed primitives with parameters from a buffer
     * @param buffer Buffer containing DrawIndexedIndirectCommand structures
     * @param offset Byte offset into buffer
     * @param draw_count Number of draws
     * @param stride Bytes between draw commands in buffer
     */
    virtual void draw_indexed_indirect(const Buffer& buffer, u64 offset, u32 draw_count,
                                       u32 stride) = 0;

    /**
     * @brief Draw with draw count from another buffer
     * @param buffer Buffer containing DrawIndirectCommand structures
     * @param offset Byte offset into draw buffer
     * @param count_buffer Buffer containing draw count
     * @param count_offset Byte offset into count buffer
     * @param max_draw_count Maximum draws to execute
     * @param stride Bytes between draw commands
     */
    virtual void draw_indirect_count(const Buffer& buffer, u64 offset, const Buffer& count_buffer,
                                     u64 count_offset, u32 max_draw_count, u32 stride) = 0;

    /**
     * @brief Indexed draw with draw count from another buffer
     */
    virtual void draw_indexed_indirect_count(const Buffer& buffer, u64 offset,
                                             const Buffer& count_buffer, u64 count_offset,
                                             u32 max_draw_count, u32 stride) = 0;

    // ========================================================================
    // Compute Commands
    // ========================================================================

    /**
     * @brief Dispatch compute work
     * @param group_count_x Number of work groups in X
     * @param group_count_y Number of work groups in Y
     * @param group_count_z Number of work groups in Z
     */
    virtual void dispatch(u32 group_count_x, u32 group_count_y = 1, u32 group_count_z = 1) = 0;

    /**
     * @brief Dispatch compute work with parameters from a buffer
     * @param buffer Buffer containing DispatchIndirectCommand
     * @param offset Byte offset into buffer
     */
    virtual void dispatch_indirect(const Buffer& buffer, u64 offset = 0) = 0;

    // ========================================================================
    // Copy Commands
    // ========================================================================

    /**
     * @brief Copy data between buffers
     */
    virtual void copy_buffer(const Buffer& src, Buffer& dst,
                             std::span<const BufferCopyRegion> regions) = 0;

    /**
     * @brief Convenience: copy entire buffer
     */
    void copy_buffer(const Buffer& src, Buffer& dst) {
        BufferCopyRegion region{0, 0, 0}; // size=0 means entire buffer
        copy_buffer(src, dst, {&region, 1});
    }

    /**
     * @brief Copy data from buffer to texture
     */
    virtual void copy_buffer_to_texture(const Buffer& src, Texture& dst,
                                        std::span<const BufferTextureCopyRegion> regions) = 0;

    /**
     * @brief Copy data from texture to buffer
     */
    virtual void copy_texture_to_buffer(const Texture& src, Buffer& dst,
                                        std::span<const BufferTextureCopyRegion> regions) = 0;

    /**
     * @brief Copy data between textures
     */
    virtual void copy_texture(const Texture& src, Texture& dst,
                              std::span<const TextureCopyRegion> regions) = 0;

    /**
     * @brief Blit (scaled/filtered copy) between textures
     * @param src Source texture
     * @param dst Destination texture
     * @param src_region Source region
     * @param dst_region Destination region (can differ for scaling)
     * @param filter Filter mode for scaling
     */
    virtual void blit_texture(const Texture& src, Texture& dst, const TextureCopyRegion& src_region,
                              const TextureCopyRegion& dst_region,
                              Filter filter = Filter::Linear) = 0;

    // ========================================================================
    // Clear Commands (outside render pass)
    // ========================================================================

    /**
     * @brief Clear a buffer to a value
     * @param buffer Buffer to clear
     * @param offset Byte offset (must be 4-byte aligned)
     * @param size Bytes to clear (must be 4-byte aligned, 0 = entire buffer)
     * @param value 32-bit value to fill with
     */
    virtual void clear_buffer(Buffer& buffer, u64 offset, u64 size, u32 value) = 0;

    /**
     * @brief Clear a color texture to a value
     */
    virtual void clear_texture(Texture& texture, const ClearColor& color, u32 base_mip_level = 0,
                               u32 mip_level_count = UINT32_MAX, u32 base_array_layer = 0,
                               u32 array_layer_count = UINT32_MAX) = 0;

    /**
     * @brief Clear a depth-stencil texture
     */
    virtual void clear_depth_stencil(Texture& texture, const ClearDepthStencil& value,
                                     u32 base_mip_level = 0, u32 mip_level_count = UINT32_MAX,
                                     u32 base_array_layer = 0,
                                     u32 array_layer_count = UINT32_MAX) = 0;

    // ========================================================================
    // Timestamp Queries
    // ========================================================================

    /**
     * @brief Write a GPU timestamp
     * @param query_pool Query pool for timestamps
     * @param query_index Index within pool
     * @param stages Pipeline stage to wait for before writing
     */
    // virtual void write_timestamp(QueryPool& pool, u32 index, ShaderStage stages) = 0;

    // ========================================================================
    // Debug Markers
    // ========================================================================

    /**
     * @brief Begin a debug marker region
     * @param name Region name (visible in GPU debuggers like RenderDoc)
     * @param color Optional color (r, g, b, a in [0,1])
     */
    virtual void begin_debug_marker(const char* name, const f32 color[4] = nullptr) = 0;

    /**
     * @brief End the current debug marker region
     */
    virtual void end_debug_marker() = 0;

    /**
     * @brief Insert a single debug marker point
     */
    virtual void insert_debug_marker(const char* name, const f32 color[4] = nullptr) = 0;

    // ========================================================================
    // Native Handle
    // ========================================================================

    /**
     * @brief Get the backend-specific native handle
     * @return Vulkan: VkCommandBuffer, DX12: ID3D12GraphicsCommandList*, OpenGL: internal ID
     */
    [[nodiscard]] virtual u64 native_handle() const noexcept = 0;

protected:
    CommandList() = default;
    HZ_NON_COPYABLE(CommandList);
    HZ_NON_MOVABLE(CommandList);
};

// ============================================================================
// Scoped Debug Marker (RAII)
// ============================================================================

/**
 * @brief RAII wrapper for debug marker regions
 *
 * Usage:
 * @code
 * {
 *     ScopedDebugMarker marker(cmd, "Shadow Pass");
 *     // ... render shadow map ...
 * } // marker automatically ends
 * @endcode
 */
class ScopedDebugMarker {
public:
    ScopedDebugMarker(CommandList& cmd, const char* name, const f32 color[4] = nullptr)
        : m_cmd(cmd) {
        m_cmd.begin_debug_marker(name, color);
    }

    ~ScopedDebugMarker() { m_cmd.end_debug_marker(); }

    HZ_NON_COPYABLE(ScopedDebugMarker);
    HZ_NON_MOVABLE(ScopedDebugMarker);

private:
    CommandList& m_cmd;
};

// Macro for convenient scoped markers with unique variable names
#define HZ_DEBUG_MARKER(cmd, name)                                                                 \
    ::hz::rhi::ScopedDebugMarker HZ_CONCAT(_debug_marker_, __LINE__)(cmd, name)

#define HZ_DEBUG_MARKER_COLOR(cmd, name, r, g, b)                                                  \
    const f32 HZ_CONCAT(_debug_color_, __LINE__)[4] = {r, g, b, 1.0f};                             \
    ::hz::rhi::ScopedDebugMarker HZ_CONCAT(_debug_marker_, __LINE__)(                              \
        cmd, name, HZ_CONCAT(_debug_color_, __LINE__))

} // namespace hz::rhi
