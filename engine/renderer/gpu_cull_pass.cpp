#include "gpu_cull_pass.hpp"

#include "engine/core/log.hpp"
#include "engine/renderer/gpu_scene.hpp"
#include "engine/rhi/rhi_command_list.hpp"

namespace hz {

namespace {

// Extract 6 frustum planes (left, right, bottom, top, near, far) from a row-major
// view-projection matrix using Gribb-Hartmann. GLM stores matrices column-major,
// so we read the rows by indexing the columns.
std::array<glm::vec4, 6> extract_frustum_planes(const glm::mat4& vp) {
    auto row = [&](int i) {
        return glm::vec4(vp[0][i], vp[1][i], vp[2][i], vp[3][i]);
    };
    const glm::vec4 r0 = row(0);
    const glm::vec4 r1 = row(1);
    const glm::vec4 r2 = row(2);
    const glm::vec4 r3 = row(3);

    std::array<glm::vec4, 6> planes{
        r3 + r0, // Left
        r3 - r0, // Right
        r3 + r1, // Bottom
        r3 - r1, // Top
        r3 + r2, // Near
        r3 - r2, // Far
    };

    for (auto& p : planes) {
        const float len = glm::length(glm::vec3(p));
        if (len > 0.0f) p /= len;
    }
    return planes;
}

} // namespace

GpuCullPass::GpuCullPass(rhi::Device& device, const GPUScene& scene) : m_device(device) {
    // Descriptor set layout: 4 storage buffers (object, mesh_info, draw_cmd, draw_count)
    {
        rhi::DescriptorSetLayoutDesc desc;
        desc.bindings.push_back(rhi::DescriptorBinding::storage_buffer(0, rhi::ShaderStage::Compute));
        desc.bindings.push_back(rhi::DescriptorBinding::storage_buffer(1, rhi::ShaderStage::Compute));
        desc.bindings.push_back(rhi::DescriptorBinding::storage_buffer(2, rhi::ShaderStage::Compute));
        desc.bindings.push_back(rhi::DescriptorBinding::storage_buffer(3, rhi::ShaderStage::Compute));
        desc.debug_name = "GpuCullSetLayout";
        m_set_layout = device.create_descriptor_set_layout(desc);
    }

    // Pipeline layout with push constants
    {
        rhi::PipelineLayoutDesc desc;
        desc.set_layouts = {m_set_layout.get()};
        desc.push_constant_ranges.push_back(
            {rhi::ShaderStage::Compute, 0, sizeof(PushConstants)});
        desc.debug_name = "GpuCullPipelineLayout";
        m_pipeline_layout = device.create_pipeline_layout(desc);
    }

    // Compute pipeline
    auto cs = device.create_shader_from_file("assets/shaders/compute/cull.comp",
                                             rhi::ShaderStage::Compute, "GpuCullCS");
    if (!cs) {
        HZ_LOG_ERROR("GpuCullPass: failed to compile cull.comp");
        return;
    }

    rhi::ComputePipelineDesc pdesc{};
    pdesc.compute_shader = cs.get();
    pdesc.layout = m_pipeline_layout.get();
    pdesc.debug_name = "GpuCullPipeline";
    m_pipeline = device.create_compute_pipeline(pdesc);

    // Descriptor pool + set
    {
        rhi::DescriptorPoolDesc desc{};
        desc.pool_sizes = {{rhi::DescriptorType::StorageBuffer, 4}};
        desc.max_sets = 1;
        desc.debug_name = "GpuCullPool";
        m_pool = device.create_descriptor_pool(desc);
    }
    m_set = m_pool->allocate(*m_set_layout);

    rhi::DescriptorWrite writes[4]{
        rhi::DescriptorWrite::storage_buffer(0, *scene.object_buffer()),
        rhi::DescriptorWrite::storage_buffer(1, *scene.mesh_info_buffer()),
        rhi::DescriptorWrite::storage_buffer(2, *scene.draw_command_buffer()),
        rhi::DescriptorWrite::storage_buffer(3, *scene.draw_count_buffer()),
    };
    m_set->write({writes, 4});

    HZ_LOG_INFO("GpuCullPass initialized");
}

GpuCullPass::~GpuCullPass() = default;

void GpuCullPass::execute(rhi::CommandList& cmd, const glm::mat4& view,
                          const glm::mat4& projection, const GPUScene& scene) {
    if (!m_pipeline || scene.object_count() == 0) return;

    HZ_DEBUG_MARKER(cmd, "GPU Cull");

    // 1. Transition count buffer to CopyDest, then clear to 0.
    //    Source state may be Undefined (first frame) or IndirectArgument (subsequent frames).
    rhi::BufferBarrier to_copy{};
    to_copy.buffer = scene.draw_count_buffer();
    to_copy.old_state = m_count_in_indirect_state ? rhi::ResourceState::IndirectArgument
                                                  : rhi::ResourceState::Undefined;
    to_copy.new_state = rhi::ResourceState::CopyDest;
    cmd.barrier(to_copy);

    cmd.clear_buffer(*scene.draw_count_buffer(), 0, sizeof(u32), 0u);

    // 2. Barrier: CopyDest -> UnorderedAccess (compute reads/writes count atomically)
    rhi::BufferBarrier to_uav{};
    to_uav.buffer = scene.draw_count_buffer();
    to_uav.old_state = rhi::ResourceState::CopyDest;
    to_uav.new_state = rhi::ResourceState::UnorderedAccess;
    cmd.barrier(to_uav);

    // Also transition draw command buffer to UnorderedAccess if it was previously IndirectArgument.
    if (m_cmd_in_indirect_state) {
        rhi::BufferBarrier to_uav_cmd{};
        to_uav_cmd.buffer = scene.draw_command_buffer();
        to_uav_cmd.old_state = rhi::ResourceState::IndirectArgument;
        to_uav_cmd.new_state = rhi::ResourceState::UnorderedAccess;
        cmd.barrier(to_uav_cmd);
    }

    // 3. Bind pipeline + descriptor set
    cmd.bind_pipeline(*m_pipeline);
    cmd.bind_descriptor_set(*m_pipeline_layout, 0, *m_set);

    // 4. Push constants (view-proj + planes + count)
    PushConstants pc{};
    pc.view_proj = projection * view;
    auto planes = extract_frustum_planes(pc.view_proj);
    for (int i = 0; i < 6; ++i) pc.frustum_planes[i] = planes[i];
    pc.object_count = scene.object_count();
    cmd.push_constants(*m_pipeline_layout, rhi::ShaderStage::Compute, 0, sizeof(pc), &pc);

    // 5. Dispatch (64 threads per group, ceil-div)
    const u32 group_count = (pc.object_count + 63) / 64;
    cmd.dispatch(group_count, 1, 1);

    // 6. Barriers: compute writes -> indirect reads (for the upcoming draw_indexed_indirect_count)
    rhi::BufferBarrier post_barriers[2]{};
    post_barriers[0].buffer = scene.draw_command_buffer();
    post_barriers[0].old_state = rhi::ResourceState::UnorderedAccess;
    post_barriers[0].new_state = rhi::ResourceState::IndirectArgument;
    post_barriers[1].buffer = scene.draw_count_buffer();
    post_barriers[1].old_state = rhi::ResourceState::UnorderedAccess;
    post_barriers[1].new_state = rhi::ResourceState::IndirectArgument;
    cmd.barriers({post_barriers, 2});

    m_count_in_indirect_state = true;
    m_cmd_in_indirect_state = true;
}

} // namespace hz
