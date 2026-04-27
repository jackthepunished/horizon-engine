#pragma once

#include "engine/core/types.hpp"
#include "engine/rhi/rhi_descriptor.hpp"
#include "engine/rhi/rhi_device.hpp"
#include "engine/rhi/rhi_pipeline.hpp"

#include <memory>

#include <glm/glm.hpp>

namespace hz {

namespace rhi {
class CommandList;
}

class GPUScene;

class GpuCullPass {
public:
    GpuCullPass(rhi::Device& device, const GPUScene& scene);
    ~GpuCullPass();

    HZ_NON_COPYABLE(GpuCullPass);
    HZ_NON_MOVABLE(GpuCullPass);

    void execute(rhi::CommandList& cmd, const glm::mat4& view, const glm::mat4& projection,
                 const GPUScene& scene);

    [[nodiscard]] bool is_valid() const noexcept { return m_pipeline != nullptr; }

private:
    struct PushConstants {
        glm::mat4 view_proj;
        glm::vec4 frustum_planes[6];
        u32 object_count;
        u32 _pad[3];
    };

    rhi::Device& m_device;
    std::unique_ptr<rhi::DescriptorSetLayout> m_set_layout;
    std::unique_ptr<rhi::PipelineLayout> m_pipeline_layout;
    std::unique_ptr<rhi::Pipeline> m_pipeline;
    std::unique_ptr<rhi::DescriptorPool> m_pool;
    std::unique_ptr<rhi::DescriptorSet> m_set;

    // Tracks whether scene buffers are currently in IndirectArgument state from a prior frame.
    bool m_count_in_indirect_state{false};
    bool m_cmd_in_indirect_state{false};
};

} // namespace hz
