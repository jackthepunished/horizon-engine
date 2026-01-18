/**
 * @file vk_command_list.cpp
 * @brief Vulkan Command List (Command Buffer) implementation
 */

#include "vk_command_list.hpp"

#include "vk_buffer.hpp"
#include "vk_descriptor.hpp"
#include "vk_device.hpp"
#include "vk_pipeline.hpp"
#include "vk_texture.hpp"

namespace hz::rhi::vk {

// ============================================================================
// Constructor / Destructor
// ============================================================================

VulkanCommandList::VulkanCommandList(VulkanDevice& device, QueueType queue_type)
    : m_device(device), m_queue_type(queue_type) {

    // Create command pool for this command list
    VkCommandPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_info.queueFamilyIndex = m_device.get_queue_family(queue_type);

    VK_CHECK_FATAL(vkCreateCommandPool(m_device.device(), &pool_info, nullptr, &m_command_pool));

    // Allocate command buffer
    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = m_command_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = 1;

    VK_CHECK_FATAL(vkAllocateCommandBuffers(m_device.device(), &alloc_info, &m_command_buffer));
}

VulkanCommandList::~VulkanCommandList() {
    if (m_command_pool != VK_NULL_HANDLE) {
        // Command buffers are freed implicitly when pool is destroyed
        vkDestroyCommandPool(m_device.device(), m_command_pool, nullptr);
    }
}

// ============================================================================
// Recording Control
// ============================================================================

void VulkanCommandList::begin() {
    HZ_ASSERT(!m_is_recording, "Command list is already recording");

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    VK_CHECK_FATAL(vkBeginCommandBuffer(m_command_buffer, &begin_info));
    m_is_recording = true;
}

void VulkanCommandList::end() {
    HZ_ASSERT(m_is_recording, "Command list is not recording");
    HZ_ASSERT(!m_inside_render_pass, "Cannot end command list inside render pass");

    VK_CHECK_FATAL(vkEndCommandBuffer(m_command_buffer));
    m_is_recording = false;
}

void VulkanCommandList::reset() {
    HZ_ASSERT(!m_is_recording, "Cannot reset command list while recording");

    VK_CHECK(vkResetCommandBuffer(m_command_buffer, 0));
    m_bound_pipeline = nullptr;
}

// ============================================================================
// Resource Barriers
// ============================================================================

void VulkanCommandList::barrier(const MemoryBarrier& barrier) {
    VkMemoryBarrier vk_barrier{};
    vk_barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    vk_barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
    vk_barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;

    VkPipelineStageFlags src_stage = to_vk_shader_stages(barrier.src_stages);
    VkPipelineStageFlags dst_stage = to_vk_shader_stages(barrier.dst_stages);

    // Convert shader stages to pipeline stages
    if (src_stage == 0)
        src_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    if (dst_stage == 0)
        dst_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

    vkCmdPipelineBarrier(m_command_buffer, src_stage, dst_stage, 0, 1, &vk_barrier, 0, nullptr, 0,
                         nullptr);
}

void VulkanCommandList::barrier(const BufferBarrier& barrier) {
    auto* vk_buffer = static_cast<const VulkanBuffer*>(barrier.buffer);

    VkBufferMemoryBarrier vk_barrier{};
    vk_barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    vk_barrier.srcAccessMask = to_vk_access_flags(barrier.old_state);
    vk_barrier.dstAccessMask = to_vk_access_flags(barrier.new_state);
    vk_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    vk_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    vk_barrier.buffer = vk_buffer->buffer();
    vk_barrier.offset = barrier.offset;
    vk_barrier.size = (barrier.size == UINT64_MAX) ? VK_WHOLE_SIZE : barrier.size;

    vkCmdPipelineBarrier(m_command_buffer, to_vk_pipeline_stage(barrier.old_state),
                         to_vk_pipeline_stage(barrier.new_state), 0, 0, nullptr, 1, &vk_barrier, 0,
                         nullptr);
}

void VulkanCommandList::barrier(const TextureBarrier& barrier) {
    auto* vk_texture = static_cast<const VulkanTexture*>(barrier.texture);

    VkImageMemoryBarrier vk_barrier{};
    vk_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    vk_barrier.srcAccessMask = to_vk_access_flags(barrier.old_state);
    vk_barrier.dstAccessMask = to_vk_access_flags(barrier.new_state);
    vk_barrier.oldLayout = to_vk_image_layout(barrier.old_state);
    vk_barrier.newLayout = to_vk_image_layout(barrier.new_state);
    vk_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    vk_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    vk_barrier.image = vk_texture->image();
    vk_barrier.subresourceRange.aspectMask = get_image_aspect(vk_texture->format());
    vk_barrier.subresourceRange.baseMipLevel = barrier.base_mip_level;
    vk_barrier.subresourceRange.levelCount =
        (barrier.mip_level_count == UINT32_MAX) ? VK_REMAINING_MIP_LEVELS : barrier.mip_level_count;
    vk_barrier.subresourceRange.baseArrayLayer = barrier.base_array_layer;
    vk_barrier.subresourceRange.layerCount = (barrier.array_layer_count == UINT32_MAX)
                                                 ? VK_REMAINING_ARRAY_LAYERS
                                                 : barrier.array_layer_count;

    vkCmdPipelineBarrier(m_command_buffer, to_vk_pipeline_stage(barrier.old_state),
                         to_vk_pipeline_stage(barrier.new_state), 0, 0, nullptr, 0, nullptr, 1,
                         &vk_barrier);
}

void VulkanCommandList::barriers(std::span<const BufferBarrier> buffer_barriers) {
    barriers(buffer_barriers, {});
}

void VulkanCommandList::barriers(std::span<const TextureBarrier> texture_barriers) {
    barriers({}, texture_barriers);
}

void VulkanCommandList::barriers(std::span<const BufferBarrier> buffer_barriers,
                                 std::span<const TextureBarrier> texture_barriers) {
    if (buffer_barriers.empty() && texture_barriers.empty()) {
        return;
    }

    std::vector<VkBufferMemoryBarrier> vk_buffer_barriers;
    std::vector<VkImageMemoryBarrier> vk_image_barriers;
    vk_buffer_barriers.reserve(buffer_barriers.size());
    vk_image_barriers.reserve(texture_barriers.size());

    VkPipelineStageFlags src_stage_mask = 0;
    VkPipelineStageFlags dst_stage_mask = 0;

    for (const auto& barrier : buffer_barriers) {
        auto* vk_buffer = static_cast<const VulkanBuffer*>(barrier.buffer);

        VkBufferMemoryBarrier vk_barrier{};
        vk_barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        vk_barrier.srcAccessMask = to_vk_access_flags(barrier.old_state);
        vk_barrier.dstAccessMask = to_vk_access_flags(barrier.new_state);
        vk_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        vk_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        vk_barrier.buffer = vk_buffer->buffer();
        vk_barrier.offset = barrier.offset;
        vk_barrier.size = (barrier.size == UINT64_MAX) ? VK_WHOLE_SIZE : barrier.size;

        vk_buffer_barriers.push_back(vk_barrier);

        src_stage_mask |= to_vk_pipeline_stage(barrier.old_state);
        dst_stage_mask |= to_vk_pipeline_stage(barrier.new_state);
    }

    for (const auto& barrier : texture_barriers) {
        auto* vk_texture = static_cast<const VulkanTexture*>(barrier.texture);

        VkImageMemoryBarrier vk_barrier{};
        vk_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        vk_barrier.srcAccessMask = to_vk_access_flags(barrier.old_state);
        vk_barrier.dstAccessMask = to_vk_access_flags(barrier.new_state);
        vk_barrier.oldLayout = to_vk_image_layout(barrier.old_state);
        vk_barrier.newLayout = to_vk_image_layout(barrier.new_state);
        vk_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        vk_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        vk_barrier.image = vk_texture->image();
        vk_barrier.subresourceRange.aspectMask = get_image_aspect(vk_texture->format());
        vk_barrier.subresourceRange.baseMipLevel = barrier.base_mip_level;
        vk_barrier.subresourceRange.levelCount = (barrier.mip_level_count == UINT32_MAX)
                                                     ? VK_REMAINING_MIP_LEVELS
                                                     : barrier.mip_level_count;
        vk_barrier.subresourceRange.baseArrayLayer = barrier.base_array_layer;
        vk_barrier.subresourceRange.layerCount = (barrier.array_layer_count == UINT32_MAX)
                                                     ? VK_REMAINING_ARRAY_LAYERS
                                                     : barrier.array_layer_count;

        vk_image_barriers.push_back(vk_barrier);

        src_stage_mask |= to_vk_pipeline_stage(barrier.old_state);
        dst_stage_mask |= to_vk_pipeline_stage(barrier.new_state);
    }

    if (src_stage_mask == 0)
        src_stage_mask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    if (dst_stage_mask == 0)
        dst_stage_mask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

    vkCmdPipelineBarrier(m_command_buffer, src_stage_mask, dst_stage_mask, 0, 0, nullptr,
                         static_cast<u32>(vk_buffer_barriers.size()), vk_buffer_barriers.data(),
                         static_cast<u32>(vk_image_barriers.size()), vk_image_barriers.data());
}

// ============================================================================
// Render Pass Commands
// ============================================================================

void VulkanCommandList::begin_render_pass(const RenderPassBeginInfo& info) {
    HZ_ASSERT(m_is_recording, "Command list is not recording");
    HZ_ASSERT(!m_inside_render_pass, "Already inside a render pass");
    HZ_ASSERT(info.framebuffer != nullptr, "Framebuffer is null");

    auto* vk_framebuffer = static_cast<const VulkanFramebuffer*>(info.framebuffer);
    auto* vk_render_pass = static_cast<const VulkanRenderPass*>(&vk_framebuffer->render_pass());

    // Build clear values
    std::vector<VkClearValue> vk_clear_values;
    vk_clear_values.reserve(info.clear_values.size());

    for (const auto& clear : info.clear_values) {
        VkClearValue vk_clear{};
        if (std::holds_alternative<ClearColor>(clear)) {
            const auto& color = std::get<ClearColor>(clear);
            vk_clear.color.float32[0] = color.r;
            vk_clear.color.float32[1] = color.g;
            vk_clear.color.float32[2] = color.b;
            vk_clear.color.float32[3] = color.a;
        } else {
            const auto& ds = std::get<ClearDepthStencil>(clear);
            vk_clear.depthStencil.depth = ds.depth;
            vk_clear.depthStencil.stencil = ds.stencil;
        }
        vk_clear_values.push_back(vk_clear);
    }

    VkRenderPassBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    begin_info.renderPass = vk_render_pass->render_pass();
    begin_info.framebuffer = vk_framebuffer->framebuffer();

    // Render area
    if (info.render_area.width > 0 && info.render_area.height > 0) {
        begin_info.renderArea.offset = {info.render_area.x, info.render_area.y};
        begin_info.renderArea.extent = {info.render_area.width, info.render_area.height};
    } else {
        begin_info.renderArea.offset = {0, 0};
        begin_info.renderArea.extent = {vk_framebuffer->width(), vk_framebuffer->height()};
    }

    begin_info.clearValueCount = static_cast<u32>(vk_clear_values.size());
    begin_info.pClearValues = vk_clear_values.data();

    vkCmdBeginRenderPass(m_command_buffer, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
    m_inside_render_pass = true;
}

void VulkanCommandList::end_render_pass() {
    HZ_ASSERT(m_inside_render_pass, "Not inside a render pass");

    vkCmdEndRenderPass(m_command_buffer);
    m_inside_render_pass = false;
}

void VulkanCommandList::next_subpass() {
    HZ_ASSERT(m_inside_render_pass, "Not inside a render pass");
    vkCmdNextSubpass(m_command_buffer, VK_SUBPASS_CONTENTS_INLINE);
}

// ============================================================================
// Pipeline Binding
// ============================================================================

void VulkanCommandList::bind_pipeline(const Pipeline& pipeline) {
    auto* vk_pipeline = static_cast<const VulkanPipeline*>(&pipeline);
    m_bound_pipeline = vk_pipeline;

    vkCmdBindPipeline(m_command_buffer, current_bind_point(), vk_pipeline->pipeline());
}

VkPipelineBindPoint VulkanCommandList::current_bind_point() const noexcept {
    if (m_bound_pipeline && m_bound_pipeline->is_compute()) {
        return VK_PIPELINE_BIND_POINT_COMPUTE;
    }
    return VK_PIPELINE_BIND_POINT_GRAPHICS;
}

// ============================================================================
// Descriptor Set Binding
// ============================================================================

void VulkanCommandList::bind_descriptor_sets(const PipelineLayout& layout, u32 first_set,
                                             std::span<const DescriptorSet* const> sets,
                                             std::span<const u32> dynamic_offsets) {
    auto* vk_layout = static_cast<const VulkanPipelineLayout*>(&layout);

    std::vector<VkDescriptorSet> vk_sets;
    vk_sets.reserve(sets.size());

    for (const auto* set : sets) {
        auto* vk_set = static_cast<const VulkanDescriptorSet*>(set);
        vk_sets.push_back(vk_set->set());
    }

    vkCmdBindDescriptorSets(m_command_buffer, current_bind_point(), vk_layout->layout(), first_set,
                            static_cast<u32>(vk_sets.size()), vk_sets.data(),
                            static_cast<u32>(dynamic_offsets.size()), dynamic_offsets.data());
}

// ============================================================================
// Push Constants
// ============================================================================

void VulkanCommandList::push_constants(const PipelineLayout& layout, ShaderStage stages, u32 offset,
                                       u32 size, const void* data) {
    auto* vk_layout = static_cast<const VulkanPipelineLayout*>(&layout);
    vkCmdPushConstants(m_command_buffer, vk_layout->layout(), to_vk_shader_stages(stages), offset,
                       size, data);
}

// ============================================================================
// Vertex/Index Buffer Binding
// ============================================================================

void VulkanCommandList::bind_vertex_buffers(u32 first_binding,
                                            std::span<const Buffer* const> buffers,
                                            std::span<const u64> offsets) {
    std::vector<VkBuffer> vk_buffers;
    std::vector<VkDeviceSize> vk_offsets;
    vk_buffers.reserve(buffers.size());
    vk_offsets.reserve(buffers.size());

    for (size_t i = 0; i < buffers.size(); ++i) {
        auto* vk_buffer = static_cast<const VulkanBuffer*>(buffers[i]);
        vk_buffers.push_back(vk_buffer->buffer());
        vk_offsets.push_back(i < offsets.size() ? offsets[i] : 0);
    }

    vkCmdBindVertexBuffers(m_command_buffer, first_binding, static_cast<u32>(vk_buffers.size()),
                           vk_buffers.data(), vk_offsets.data());
}

void VulkanCommandList::bind_index_buffer(const Buffer& buffer, u64 offset, IndexType type) {
    auto* vk_buffer = static_cast<const VulkanBuffer*>(&buffer);
    vkCmdBindIndexBuffer(m_command_buffer, vk_buffer->buffer(), offset, to_vk_index_type(type));
}

// ============================================================================
// Dynamic State
// ============================================================================

void VulkanCommandList::set_viewport(const Viewport& viewport) {
    VkViewport vk_viewport{};
    vk_viewport.x = viewport.x;
    vk_viewport.y = viewport.y;
    vk_viewport.width = viewport.width;
    vk_viewport.height = viewport.height;
    vk_viewport.minDepth = viewport.min_depth;
    vk_viewport.maxDepth = viewport.max_depth;

    vkCmdSetViewport(m_command_buffer, 0, 1, &vk_viewport);
}

void VulkanCommandList::set_viewports(u32 first_viewport, std::span<const Viewport> viewports) {
    std::vector<VkViewport> vk_viewports;
    vk_viewports.reserve(viewports.size());

    for (const auto& vp : viewports) {
        VkViewport vk_vp{};
        vk_vp.x = vp.x;
        vk_vp.y = vp.y;
        vk_vp.width = vp.width;
        vk_vp.height = vp.height;
        vk_vp.minDepth = vp.min_depth;
        vk_vp.maxDepth = vp.max_depth;
        vk_viewports.push_back(vk_vp);
    }

    vkCmdSetViewport(m_command_buffer, first_viewport, static_cast<u32>(vk_viewports.size()),
                     vk_viewports.data());
}

void VulkanCommandList::set_scissor(const Scissor& scissor) {
    VkRect2D vk_scissor{};
    vk_scissor.offset = {scissor.x, scissor.y};
    vk_scissor.extent = {scissor.width, scissor.height};

    vkCmdSetScissor(m_command_buffer, 0, 1, &vk_scissor);
}

void VulkanCommandList::set_scissors(u32 first_scissor, std::span<const Scissor> scissors) {
    std::vector<VkRect2D> vk_scissors;
    vk_scissors.reserve(scissors.size());

    for (const auto& sc : scissors) {
        VkRect2D vk_sc{};
        vk_sc.offset = {sc.x, sc.y};
        vk_sc.extent = {sc.width, sc.height};
        vk_scissors.push_back(vk_sc);
    }

    vkCmdSetScissor(m_command_buffer, first_scissor, static_cast<u32>(vk_scissors.size()),
                    vk_scissors.data());
}

void VulkanCommandList::set_blend_constants(const f32 constants[4]) {
    vkCmdSetBlendConstants(m_command_buffer, constants);
}

void VulkanCommandList::set_depth_bias(f32 constant_factor, f32 clamp, f32 slope_factor) {
    vkCmdSetDepthBias(m_command_buffer, constant_factor, clamp, slope_factor);
}

void VulkanCommandList::set_stencil_reference(u32 reference) {
    vkCmdSetStencilReference(m_command_buffer, VK_STENCIL_FACE_FRONT_AND_BACK, reference);
}

void VulkanCommandList::set_line_width(f32 width) {
    vkCmdSetLineWidth(m_command_buffer, width);
}

// ============================================================================
// Draw Commands
// ============================================================================

void VulkanCommandList::draw(u32 vertex_count, u32 instance_count, u32 first_vertex,
                             u32 first_instance) {
    vkCmdDraw(m_command_buffer, vertex_count, instance_count, first_vertex, first_instance);
}

void VulkanCommandList::draw_indexed(u32 index_count, u32 instance_count, u32 first_index,
                                     i32 vertex_offset, u32 first_instance) {
    vkCmdDrawIndexed(m_command_buffer, index_count, instance_count, first_index, vertex_offset,
                     first_instance);
}

void VulkanCommandList::draw_indirect(const Buffer& buffer, u64 offset, u32 draw_count,
                                      u32 stride) {
    auto* vk_buffer = static_cast<const VulkanBuffer*>(&buffer);
    vkCmdDrawIndirect(m_command_buffer, vk_buffer->buffer(), offset, draw_count, stride);
}

void VulkanCommandList::draw_indexed_indirect(const Buffer& buffer, u64 offset, u32 draw_count,
                                              u32 stride) {
    auto* vk_buffer = static_cast<const VulkanBuffer*>(&buffer);
    vkCmdDrawIndexedIndirect(m_command_buffer, vk_buffer->buffer(), offset, draw_count, stride);
}

void VulkanCommandList::draw_indirect_count(const Buffer& buffer, u64 offset,
                                            const Buffer& count_buffer, u64 count_offset,
                                            u32 max_draw_count, u32 stride) {
    auto* vk_buffer = static_cast<const VulkanBuffer*>(&buffer);
    auto* vk_count_buffer = static_cast<const VulkanBuffer*>(&count_buffer);
    vkCmdDrawIndirectCount(m_command_buffer, vk_buffer->buffer(), offset, vk_count_buffer->buffer(),
                           count_offset, max_draw_count, stride);
}

void VulkanCommandList::draw_indexed_indirect_count(const Buffer& buffer, u64 offset,
                                                    const Buffer& count_buffer, u64 count_offset,
                                                    u32 max_draw_count, u32 stride) {
    auto* vk_buffer = static_cast<const VulkanBuffer*>(&buffer);
    auto* vk_count_buffer = static_cast<const VulkanBuffer*>(&count_buffer);
    vkCmdDrawIndexedIndirectCount(m_command_buffer, vk_buffer->buffer(), offset,
                                  vk_count_buffer->buffer(), count_offset, max_draw_count, stride);
}

// ============================================================================
// Compute Commands
// ============================================================================

void VulkanCommandList::dispatch(u32 group_count_x, u32 group_count_y, u32 group_count_z) {
    vkCmdDispatch(m_command_buffer, group_count_x, group_count_y, group_count_z);
}

void VulkanCommandList::dispatch_indirect(const Buffer& buffer, u64 offset) {
    auto* vk_buffer = static_cast<const VulkanBuffer*>(&buffer);
    vkCmdDispatchIndirect(m_command_buffer, vk_buffer->buffer(), offset);
}

// ============================================================================
// Copy Commands
// ============================================================================

void VulkanCommandList::copy_buffer(const Buffer& src, Buffer& dst,
                                    std::span<const BufferCopyRegion> regions) {
    auto* vk_src = static_cast<const VulkanBuffer*>(&src);
    auto* vk_dst = static_cast<VulkanBuffer*>(&dst);

    std::vector<VkBufferCopy> vk_regions;
    vk_regions.reserve(regions.size());

    for (const auto& region : regions) {
        VkBufferCopy vk_region{};
        vk_region.srcOffset = region.src_offset;
        vk_region.dstOffset = region.dst_offset;
        vk_region.size = (region.size == 0) ? vk_src->size() : region.size;
        vk_regions.push_back(vk_region);
    }

    vkCmdCopyBuffer(m_command_buffer, vk_src->buffer(), vk_dst->buffer(),
                    static_cast<u32>(vk_regions.size()), vk_regions.data());
}

void VulkanCommandList::copy_buffer_to_texture(const Buffer& src, Texture& dst,
                                               std::span<const BufferTextureCopyRegion> regions) {
    auto* vk_src = static_cast<const VulkanBuffer*>(&src);
    auto* vk_dst = static_cast<VulkanTexture*>(&dst);

    std::vector<VkBufferImageCopy> vk_regions;
    vk_regions.reserve(regions.size());

    for (const auto& region : regions) {
        VkBufferImageCopy vk_region{};
        vk_region.bufferOffset = region.buffer_offset;
        vk_region.bufferRowLength = region.buffer_row_length;
        vk_region.bufferImageHeight = region.buffer_image_height;
        vk_region.imageSubresource.aspectMask = get_image_aspect(vk_dst->format());
        vk_region.imageSubresource.mipLevel = region.mip_level;
        vk_region.imageSubresource.baseArrayLayer = region.base_array_layer;
        vk_region.imageSubresource.layerCount = region.layer_count;
        vk_region.imageOffset = {static_cast<i32>(region.texture_offset.x),
                                 static_cast<i32>(region.texture_offset.y),
                                 static_cast<i32>(region.texture_offset.z)};
        vk_region.imageExtent = {region.texture_extent.width, region.texture_extent.height,
                                 region.texture_extent.depth};
        vk_regions.push_back(vk_region);
    }

    vkCmdCopyBufferToImage(m_command_buffer, vk_src->buffer(), vk_dst->image(),
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           static_cast<u32>(vk_regions.size()), vk_regions.data());
}

void VulkanCommandList::copy_texture_to_buffer(const Texture& src, Buffer& dst,
                                               std::span<const BufferTextureCopyRegion> regions) {
    auto* vk_src = static_cast<const VulkanTexture*>(&src);
    auto* vk_dst = static_cast<VulkanBuffer*>(&dst);

    std::vector<VkBufferImageCopy> vk_regions;
    vk_regions.reserve(regions.size());

    for (const auto& region : regions) {
        VkBufferImageCopy vk_region{};
        vk_region.bufferOffset = region.buffer_offset;
        vk_region.bufferRowLength = region.buffer_row_length;
        vk_region.bufferImageHeight = region.buffer_image_height;
        vk_region.imageSubresource.aspectMask = get_image_aspect(vk_src->format());
        vk_region.imageSubresource.mipLevel = region.mip_level;
        vk_region.imageSubresource.baseArrayLayer = region.base_array_layer;
        vk_region.imageSubresource.layerCount = region.layer_count;
        vk_region.imageOffset = {static_cast<i32>(region.texture_offset.x),
                                 static_cast<i32>(region.texture_offset.y),
                                 static_cast<i32>(region.texture_offset.z)};
        vk_region.imageExtent = {region.texture_extent.width, region.texture_extent.height,
                                 region.texture_extent.depth};
        vk_regions.push_back(vk_region);
    }

    vkCmdCopyImageToBuffer(m_command_buffer, vk_src->image(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           vk_dst->buffer(), static_cast<u32>(vk_regions.size()),
                           vk_regions.data());
}

void VulkanCommandList::copy_texture(const Texture& src, Texture& dst,
                                     std::span<const TextureCopyRegion> regions) {
    auto* vk_src = static_cast<const VulkanTexture*>(&src);
    auto* vk_dst = static_cast<VulkanTexture*>(&dst);

    std::vector<VkImageCopy> vk_regions;
    vk_regions.reserve(regions.size());

    for (const auto& region : regions) {
        VkImageCopy vk_region{};
        vk_region.srcSubresource.aspectMask = get_image_aspect(vk_src->format());
        vk_region.srcSubresource.mipLevel = region.src_mip_level;
        vk_region.srcSubresource.baseArrayLayer = region.src_base_array_layer;
        vk_region.srcSubresource.layerCount = region.src_layer_count;
        vk_region.srcOffset = {static_cast<i32>(region.src_offset.x),
                               static_cast<i32>(region.src_offset.y),
                               static_cast<i32>(region.src_offset.z)};
        vk_region.dstSubresource.aspectMask = get_image_aspect(vk_dst->format());
        vk_region.dstSubresource.mipLevel = region.dst_mip_level;
        vk_region.dstSubresource.baseArrayLayer = region.dst_base_array_layer;
        vk_region.dstSubresource.layerCount = region.dst_layer_count;
        vk_region.dstOffset = {static_cast<i32>(region.dst_offset.x),
                               static_cast<i32>(region.dst_offset.y),
                               static_cast<i32>(region.dst_offset.z)};
        vk_region.extent = {region.extent.width, region.extent.height, region.extent.depth};
        vk_regions.push_back(vk_region);
    }

    vkCmdCopyImage(m_command_buffer, vk_src->image(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   vk_dst->image(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                   static_cast<u32>(vk_regions.size()), vk_regions.data());
}

void VulkanCommandList::blit_texture(const Texture& src, Texture& dst,
                                     const TextureCopyRegion& src_region,
                                     const TextureCopyRegion& dst_region, Filter filter) {
    auto* vk_src = static_cast<const VulkanTexture*>(&src);
    auto* vk_dst = static_cast<VulkanTexture*>(&dst);

    VkImageBlit blit{};
    blit.srcSubresource.aspectMask = get_image_aspect(vk_src->format());
    blit.srcSubresource.mipLevel = src_region.src_mip_level;
    blit.srcSubresource.baseArrayLayer = src_region.src_base_array_layer;
    blit.srcSubresource.layerCount = src_region.src_layer_count;
    blit.srcOffsets[0] = {static_cast<i32>(src_region.src_offset.x),
                          static_cast<i32>(src_region.src_offset.y),
                          static_cast<i32>(src_region.src_offset.z)};
    blit.srcOffsets[1] = {static_cast<i32>(src_region.src_offset.x + src_region.extent.width),
                          static_cast<i32>(src_region.src_offset.y + src_region.extent.height),
                          static_cast<i32>(src_region.src_offset.z + src_region.extent.depth)};

    blit.dstSubresource.aspectMask = get_image_aspect(vk_dst->format());
    blit.dstSubresource.mipLevel = dst_region.dst_mip_level;
    blit.dstSubresource.baseArrayLayer = dst_region.dst_base_array_layer;
    blit.dstSubresource.layerCount = dst_region.dst_layer_count;
    blit.dstOffsets[0] = {static_cast<i32>(dst_region.dst_offset.x),
                          static_cast<i32>(dst_region.dst_offset.y),
                          static_cast<i32>(dst_region.dst_offset.z)};
    blit.dstOffsets[1] = {static_cast<i32>(dst_region.dst_offset.x + dst_region.extent.width),
                          static_cast<i32>(dst_region.dst_offset.y + dst_region.extent.height),
                          static_cast<i32>(dst_region.dst_offset.z + dst_region.extent.depth)};

    vkCmdBlitImage(m_command_buffer, vk_src->image(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   vk_dst->image(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit,
                   to_vk_filter(filter));
}

// ============================================================================
// Clear Commands
// ============================================================================

void VulkanCommandList::clear_buffer(Buffer& buffer, u64 offset, u64 size, u32 value) {
    auto* vk_buffer = static_cast<VulkanBuffer*>(&buffer);
    VkDeviceSize vk_size = (size == 0) ? VK_WHOLE_SIZE : size;
    vkCmdFillBuffer(m_command_buffer, vk_buffer->buffer(), offset, vk_size, value);
}

void VulkanCommandList::clear_texture(Texture& texture, const ClearColor& color, u32 base_mip_level,
                                      u32 mip_level_count, u32 base_array_layer,
                                      u32 array_layer_count) {
    auto* vk_texture = static_cast<VulkanTexture*>(&texture);

    VkClearColorValue vk_color{};
    vk_color.float32[0] = color.r;
    vk_color.float32[1] = color.g;
    vk_color.float32[2] = color.b;
    vk_color.float32[3] = color.a;

    VkImageSubresourceRange range{};
    range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    range.baseMipLevel = base_mip_level;
    range.levelCount = (mip_level_count == UINT32_MAX) ? VK_REMAINING_MIP_LEVELS : mip_level_count;
    range.baseArrayLayer = base_array_layer;
    range.layerCount =
        (array_layer_count == UINT32_MAX) ? VK_REMAINING_ARRAY_LAYERS : array_layer_count;

    vkCmdClearColorImage(m_command_buffer, vk_texture->image(),
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &vk_color, 1, &range);
}

void VulkanCommandList::clear_depth_stencil(Texture& texture, const ClearDepthStencil& value,
                                            u32 base_mip_level, u32 mip_level_count,
                                            u32 base_array_layer, u32 array_layer_count) {
    auto* vk_texture = static_cast<VulkanTexture*>(&texture);

    VkClearDepthStencilValue vk_value{};
    vk_value.depth = value.depth;
    vk_value.stencil = value.stencil;

    VkImageSubresourceRange range{};
    range.aspectMask = get_image_aspect(vk_texture->format());
    range.baseMipLevel = base_mip_level;
    range.levelCount = (mip_level_count == UINT32_MAX) ? VK_REMAINING_MIP_LEVELS : mip_level_count;
    range.baseArrayLayer = base_array_layer;
    range.layerCount =
        (array_layer_count == UINT32_MAX) ? VK_REMAINING_ARRAY_LAYERS : array_layer_count;

    vkCmdClearDepthStencilImage(m_command_buffer, vk_texture->image(),
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &vk_value, 1, &range);
}

// ============================================================================
// Debug Markers
// ============================================================================

void VulkanCommandList::begin_debug_marker(const char* name, const f32 color[4]) {
    if (vkCmdBeginDebugUtilsLabelEXT) {
        VkDebugUtilsLabelEXT label{};
        label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
        label.pLabelName = name;
        if (color) {
            label.color[0] = color[0];
            label.color[1] = color[1];
            label.color[2] = color[2];
            label.color[3] = color[3];
        } else {
            label.color[0] = 1.0f;
            label.color[1] = 1.0f;
            label.color[2] = 1.0f;
            label.color[3] = 1.0f;
        }
        vkCmdBeginDebugUtilsLabelEXT(m_command_buffer, &label);
    }
}

void VulkanCommandList::end_debug_marker() {
    if (vkCmdEndDebugUtilsLabelEXT) {
        vkCmdEndDebugUtilsLabelEXT(m_command_buffer);
    }
}

void VulkanCommandList::insert_debug_marker(const char* name, const f32 color[4]) {
    if (vkCmdInsertDebugUtilsLabelEXT) {
        VkDebugUtilsLabelEXT label{};
        label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
        label.pLabelName = name;
        if (color) {
            label.color[0] = color[0];
            label.color[1] = color[1];
            label.color[2] = color[2];
            label.color[3] = color[3];
        } else {
            label.color[0] = 1.0f;
            label.color[1] = 1.0f;
            label.color[2] = 1.0f;
            label.color[3] = 1.0f;
        }
        vkCmdInsertDebugUtilsLabelEXT(m_command_buffer, &label);
    }
}

} // namespace hz::rhi::vk
