#include "vk_pipeline.hpp"

#include "vk_descriptor.hpp"
#include "vk_device.hpp"
#include "vk_texture.hpp"

namespace hz::rhi::vk {

// ============================================================================
// VulkanShaderModule Implementation
// ============================================================================

VulkanShaderModule::VulkanShaderModule(VulkanDevice& device, const ShaderModuleDesc& desc)
    : m_device(device)
    , m_stage(desc.stage)
    , m_entry_point(desc.entry_point ? desc.entry_point : "main") {

    VkShaderModuleCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = desc.bytecode.size();
    create_info.pCode = reinterpret_cast<const u32*>(desc.bytecode.data());

    VkResult result = vkCreateShaderModule(m_device.device(), &create_info, nullptr, &m_module);
    if (result != VK_SUCCESS) {
        HZ_LOG_ERROR("Failed to create Vulkan shader module: {}", vk_result_string(result));
        return;
    }

    if (desc.debug_name) {
        m_device.set_debug_name(reinterpret_cast<u64>(m_module), desc.debug_name);
    }
}

VulkanShaderModule::~VulkanShaderModule() {
    if (m_module != VK_NULL_HANDLE) {
        m_device.defer_deletion(m_module);
        m_module = VK_NULL_HANDLE;
    }
}

// ============================================================================
// VulkanRenderPass Implementation
// ============================================================================

VulkanRenderPass::VulkanRenderPass(VulkanDevice& device, const RenderPassDesc& desc)
    : m_device(device)
    , m_has_depth_stencil(desc.has_depth_stencil)
    , m_depth_stencil_format(desc.depth_stencil_attachment.format) {

    std::vector<VkAttachmentDescription> attachments;
    std::vector<VkAttachmentReference> color_refs;

    // Color attachments
    for (const auto& color_attach : desc.color_attachments) {
        m_color_formats.push_back(color_attach.format);

        VkAttachmentDescription attachment{};
        attachment.format = to_vk_format(color_attach.format);
        attachment.samples = static_cast<VkSampleCountFlagBits>(color_attach.sample_count);
        attachment.loadOp = to_vk_load_op(color_attach.load_op);
        attachment.storeOp = to_vk_store_op(color_attach.store_op);
        attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment.initialLayout = to_vk_image_layout(color_attach.initial_state);
        attachment.finalLayout = to_vk_image_layout(color_attach.final_state);
        attachments.push_back(attachment);

        VkAttachmentReference ref{};
        ref.attachment = static_cast<u32>(color_refs.size());
        ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        color_refs.push_back(ref);
    }

    // Depth-stencil attachment
    VkAttachmentReference depth_ref{};
    depth_ref.attachment = VK_ATTACHMENT_UNUSED;

    if (m_has_depth_stencil) {
        const auto& depth_attach = desc.depth_stencil_attachment;

        VkAttachmentDescription attachment{};
        attachment.format = to_vk_format(depth_attach.format);
        attachment.samples = static_cast<VkSampleCountFlagBits>(depth_attach.sample_count);
        attachment.loadOp = to_vk_load_op(depth_attach.load_op);
        attachment.storeOp = to_vk_store_op(depth_attach.store_op);
        attachment.stencilLoadOp = to_vk_load_op(depth_attach.stencil_load_op);
        attachment.stencilStoreOp = to_vk_store_op(depth_attach.stencil_store_op);
        attachment.initialLayout = to_vk_image_layout(depth_attach.initial_state);
        attachment.finalLayout = to_vk_image_layout(depth_attach.final_state);
        attachments.push_back(attachment);

        depth_ref.attachment = static_cast<u32>(attachments.size() - 1);
        depth_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    }

    // Subpass
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = static_cast<u32>(color_refs.size());
    subpass.pColorAttachments = color_refs.empty() ? nullptr : color_refs.data();
    subpass.pDepthStencilAttachment = m_has_depth_stencil ? &depth_ref : nullptr;

    // Subpass dependencies for proper synchronization
    std::array<VkSubpassDependency, 2> dependencies{};

    // External -> Subpass 0
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependencies[0].dstStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependencies[0].srcAccessMask = 0;
    dependencies[0].dstAccessMask =
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    // Subpass 0 -> External
    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependencies[1].dstStageMask =
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependencies[1].srcAccessMask =
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask =
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;

    // Create render pass
    VkRenderPassCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    create_info.attachmentCount = static_cast<u32>(attachments.size());
    create_info.pAttachments = attachments.empty() ? nullptr : attachments.data();
    create_info.subpassCount = 1;
    create_info.pSubpasses = &subpass;
    create_info.dependencyCount = static_cast<u32>(dependencies.size());
    create_info.pDependencies = dependencies.data();

    VkResult result = vkCreateRenderPass(m_device.device(), &create_info, nullptr, &m_render_pass);
    if (result != VK_SUCCESS) {
        HZ_LOG_ERROR("Failed to create Vulkan render pass: {}", vk_result_string(result));
        return;
    }

    if (desc.debug_name) {
        m_device.set_debug_name(reinterpret_cast<u64>(m_render_pass), desc.debug_name);
    }
}

VulkanRenderPass::~VulkanRenderPass() {
    if (m_render_pass != VK_NULL_HANDLE) {
        m_device.defer_deletion(m_render_pass);
        m_render_pass = VK_NULL_HANDLE;
    }
}

// ============================================================================
// VulkanFramebuffer Implementation
// ============================================================================

VulkanFramebuffer::VulkanFramebuffer(VulkanDevice& device, const FramebufferDesc& desc)
    : m_device(device)
    , m_render_pass(static_cast<const VulkanRenderPass*>(desc.render_pass))
    , m_width(desc.width)
    , m_height(desc.height)
    , m_layers(desc.layers) {

    std::vector<VkImageView> attachments;

    // Color attachments
    for (const auto* view : desc.color_attachments) {
        const auto* vk_view = static_cast<const VulkanTextureView*>(view);
        attachments.push_back(vk_view->image_view());
    }

    // Depth-stencil attachment
    if (desc.depth_stencil_attachment) {
        const auto* vk_view = static_cast<const VulkanTextureView*>(desc.depth_stencil_attachment);
        attachments.push_back(vk_view->image_view());
    }

    VkFramebufferCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    create_info.renderPass = m_render_pass->render_pass();
    create_info.attachmentCount = static_cast<u32>(attachments.size());
    create_info.pAttachments = attachments.empty() ? nullptr : attachments.data();
    create_info.width = m_width;
    create_info.height = m_height;
    create_info.layers = m_layers;

    VkResult result = vkCreateFramebuffer(m_device.device(), &create_info, nullptr, &m_framebuffer);
    if (result != VK_SUCCESS) {
        HZ_LOG_ERROR("Failed to create Vulkan framebuffer: {}", vk_result_string(result));
        return;
    }

    if (desc.debug_name) {
        m_device.set_debug_name(reinterpret_cast<u64>(m_framebuffer), desc.debug_name);
    }
}

VulkanFramebuffer::~VulkanFramebuffer() {
    if (m_framebuffer != VK_NULL_HANDLE) {
        m_device.defer_deletion(m_framebuffer);
        m_framebuffer = VK_NULL_HANDLE;
    }
}

// ============================================================================
// VulkanPipelineLayout Implementation
// ============================================================================

VulkanPipelineLayout::VulkanPipelineLayout(VulkanDevice& device, const PipelineLayoutDesc& desc)
    : m_device(device), m_descriptor_set_count(static_cast<u32>(desc.set_layouts.size())) {

    std::vector<VkDescriptorSetLayout> vk_layouts;
    for (const auto* layout : desc.set_layouts) {
        const auto* vk_layout = static_cast<const VulkanDescriptorSetLayout*>(layout);
        vk_layouts.push_back(vk_layout->layout());
    }

    std::vector<VkPushConstantRange> vk_push_constants;
    for (const auto& range : desc.push_constant_ranges) {
        VkPushConstantRange vk_range{};
        vk_range.stageFlags = to_vk_shader_stages(range.stages);
        vk_range.offset = range.offset;
        vk_range.size = range.size;
        vk_push_constants.push_back(vk_range);
        m_push_constant_size = std::max(m_push_constant_size, range.offset + range.size);
    }

    VkPipelineLayoutCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    create_info.setLayoutCount = static_cast<u32>(vk_layouts.size());
    create_info.pSetLayouts = vk_layouts.empty() ? nullptr : vk_layouts.data();
    create_info.pushConstantRangeCount = static_cast<u32>(vk_push_constants.size());
    create_info.pPushConstantRanges =
        vk_push_constants.empty() ? nullptr : vk_push_constants.data();

    VkResult result = vkCreatePipelineLayout(m_device.device(), &create_info, nullptr, &m_layout);
    if (result != VK_SUCCESS) {
        HZ_LOG_ERROR("Failed to create Vulkan pipeline layout: {}", vk_result_string(result));
        return;
    }

    if (desc.debug_name) {
        m_device.set_debug_name(reinterpret_cast<u64>(m_layout), desc.debug_name);
    }
}

VulkanPipelineLayout::~VulkanPipelineLayout() {
    if (m_layout != VK_NULL_HANDLE) {
        m_device.defer_deletion(m_layout);
        m_layout = VK_NULL_HANDLE;
    }
}

// ============================================================================
// VulkanPipeline Implementation - Graphics
// ============================================================================

VulkanPipeline::VulkanPipeline(VulkanDevice& device, const GraphicsPipelineDesc& desc)
    : m_device(device)
    , m_layout(static_cast<const VulkanPipelineLayout*>(desc.layout))
    , m_is_compute(false) {

    // Shader stages
    std::vector<VkPipelineShaderStageCreateInfo> shader_stages;

    auto add_shader_stage = [&](const ShaderModule* shader) {
        if (!shader)
            return;
        const auto* vk_shader = static_cast<const VulkanShaderModule*>(shader);
        VkPipelineShaderStageCreateInfo stage_info{};
        stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage_info.stage = to_vk_shader_stage(shader->stage());
        stage_info.module = vk_shader->module();
        stage_info.pName = vk_shader->entry_point();
        shader_stages.push_back(stage_info);
    };

    add_shader_stage(desc.vertex_shader);
    add_shader_stage(desc.fragment_shader);
    add_shader_stage(desc.geometry_shader);
    add_shader_stage(desc.tess_control_shader);
    add_shader_stage(desc.tess_eval_shader);

    // Vertex input
    std::vector<VkVertexInputBindingDescription> binding_descs;
    for (const auto& binding : desc.vertex_layout.bindings) {
        VkVertexInputBindingDescription vk_binding{};
        vk_binding.binding = binding.binding;
        vk_binding.stride = binding.stride;
        vk_binding.inputRate = binding.input_rate == VertexInputRate::Vertex
                                   ? VK_VERTEX_INPUT_RATE_VERTEX
                                   : VK_VERTEX_INPUT_RATE_INSTANCE;
        binding_descs.push_back(vk_binding);
    }

    std::vector<VkVertexInputAttributeDescription> attr_descs;
    for (const auto& attr : desc.vertex_layout.attributes) {
        VkVertexInputAttributeDescription vk_attr{};
        vk_attr.location = attr.location;
        vk_attr.binding = attr.binding;
        vk_attr.format = to_vk_format(attr.format);
        vk_attr.offset = attr.offset;
        attr_descs.push_back(vk_attr);
    }

    VkPipelineVertexInputStateCreateInfo vertex_input_info{};
    vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_info.vertexBindingDescriptionCount = static_cast<u32>(binding_descs.size());
    vertex_input_info.pVertexBindingDescriptions =
        binding_descs.empty() ? nullptr : binding_descs.data();
    vertex_input_info.vertexAttributeDescriptionCount = static_cast<u32>(attr_descs.size());
    vertex_input_info.pVertexAttributeDescriptions =
        attr_descs.empty() ? nullptr : attr_descs.data();

    // Input assembly
    VkPipelineInputAssemblyStateCreateInfo input_assembly{};
    input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = to_vk_topology(desc.topology);
    input_assembly.primitiveRestartEnable = desc.primitive_restart_enable ? VK_TRUE : VK_FALSE;

    // Viewport state (dynamic)
    VkPipelineViewportStateCreateInfo viewport_state{};
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.scissorCount = 1;

    // Rasterization
    VkPipelineRasterizationStateCreateInfo rasterization{};
    rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.depthClampEnable = desc.rasterization.depth_clamp_enable ? VK_TRUE : VK_FALSE;
    rasterization.rasterizerDiscardEnable =
        desc.rasterization.rasterizer_discard_enable ? VK_TRUE : VK_FALSE;
    rasterization.polygonMode = to_vk_polygon_mode(desc.rasterization.polygon_mode);
    rasterization.cullMode = to_vk_cull_mode(desc.rasterization.cull_mode);
    rasterization.frontFace = to_vk_front_face(desc.rasterization.front_face);
    rasterization.depthBiasEnable = desc.rasterization.depth_bias_enable ? VK_TRUE : VK_FALSE;
    rasterization.depthBiasConstantFactor = desc.rasterization.depth_bias_constant;
    rasterization.depthBiasClamp = desc.rasterization.depth_bias_clamp;
    rasterization.depthBiasSlopeFactor = desc.rasterization.depth_bias_slope;
    rasterization.lineWidth = desc.rasterization.line_width;

    // Multisample
    VkPipelineMultisampleStateCreateInfo multisample{};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples =
        static_cast<VkSampleCountFlagBits>(desc.multisample.sample_count);
    multisample.sampleShadingEnable = desc.multisample.sample_shading_enable ? VK_TRUE : VK_FALSE;
    multisample.minSampleShading = desc.multisample.min_sample_shading;
    multisample.alphaToCoverageEnable =
        desc.multisample.alpha_to_coverage_enable ? VK_TRUE : VK_FALSE;
    multisample.alphaToOneEnable = desc.multisample.alpha_to_one_enable ? VK_TRUE : VK_FALSE;

    // Depth-stencil
    VkPipelineDepthStencilStateCreateInfo depth_stencil{};
    depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil.depthTestEnable = desc.depth_stencil.depth_test_enable ? VK_TRUE : VK_FALSE;
    depth_stencil.depthWriteEnable = desc.depth_stencil.depth_write_enable ? VK_TRUE : VK_FALSE;
    depth_stencil.depthCompareOp = to_vk_compare_op(desc.depth_stencil.depth_compare_op);
    depth_stencil.depthBoundsTestEnable =
        desc.depth_stencil.depth_bounds_test_enable ? VK_TRUE : VK_FALSE;
    depth_stencil.minDepthBounds = desc.depth_stencil.min_depth_bounds;
    depth_stencil.maxDepthBounds = desc.depth_stencil.max_depth_bounds;
    depth_stencil.stencilTestEnable = desc.depth_stencil.stencil_test_enable ? VK_TRUE : VK_FALSE;

    // Stencil ops
    auto convert_stencil_op = [](const StencilOpState& state) -> VkStencilOpState {
        VkStencilOpState vk_state{};
        vk_state.failOp = to_vk_stencil_op(state.fail_op);
        vk_state.passOp = to_vk_stencil_op(state.pass_op);
        vk_state.depthFailOp = to_vk_stencil_op(state.depth_fail_op);
        vk_state.compareOp = to_vk_compare_op(state.compare_op);
        vk_state.compareMask = state.compare_mask;
        vk_state.writeMask = state.write_mask;
        vk_state.reference = state.reference;
        return vk_state;
    };
    depth_stencil.front = convert_stencil_op(desc.depth_stencil.front);
    depth_stencil.back = convert_stencil_op(desc.depth_stencil.back);

    // Color blend
    std::vector<VkPipelineColorBlendAttachmentState> blend_attachments;
    for (const auto& attach : desc.blend.attachments) {
        VkPipelineColorBlendAttachmentState vk_attach{};
        vk_attach.blendEnable = attach.blend_enable ? VK_TRUE : VK_FALSE;
        vk_attach.srcColorBlendFactor = to_vk_blend_factor(attach.src_color_factor);
        vk_attach.dstColorBlendFactor = to_vk_blend_factor(attach.dst_color_factor);
        vk_attach.colorBlendOp = to_vk_blend_op(attach.color_blend_op);
        vk_attach.srcAlphaBlendFactor = to_vk_blend_factor(attach.src_alpha_factor);
        vk_attach.dstAlphaBlendFactor = to_vk_blend_factor(attach.dst_alpha_factor);
        vk_attach.alphaBlendOp = to_vk_blend_op(attach.alpha_blend_op);
        vk_attach.colorWriteMask = to_vk_color_write_mask(attach.color_write_mask);
        blend_attachments.push_back(vk_attach);
    }

    VkPipelineColorBlendStateCreateInfo color_blend{};
    color_blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blend.logicOpEnable = desc.blend.logic_op_enable ? VK_TRUE : VK_FALSE;
    color_blend.attachmentCount = static_cast<u32>(blend_attachments.size());
    color_blend.pAttachments = blend_attachments.empty() ? nullptr : blend_attachments.data();
    std::memcpy(color_blend.blendConstants, desc.blend.blend_constants,
                sizeof(desc.blend.blend_constants));

    // Dynamic state
    std::vector<VkDynamicState> dynamic_states;
    if (desc.dynamic_viewport)
        dynamic_states.push_back(VK_DYNAMIC_STATE_VIEWPORT);
    if (desc.dynamic_scissor)
        dynamic_states.push_back(VK_DYNAMIC_STATE_SCISSOR);
    if (desc.dynamic_line_width)
        dynamic_states.push_back(VK_DYNAMIC_STATE_LINE_WIDTH);
    if (desc.dynamic_depth_bias)
        dynamic_states.push_back(VK_DYNAMIC_STATE_DEPTH_BIAS);
    if (desc.dynamic_blend_constants)
        dynamic_states.push_back(VK_DYNAMIC_STATE_BLEND_CONSTANTS);
    if (desc.dynamic_stencil_reference)
        dynamic_states.push_back(VK_DYNAMIC_STATE_STENCIL_REFERENCE);

    VkPipelineDynamicStateCreateInfo dynamic_state{};
    dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state.dynamicStateCount = static_cast<u32>(dynamic_states.size());
    dynamic_state.pDynamicStates = dynamic_states.empty() ? nullptr : dynamic_states.data();

    // Create pipeline
    VkGraphicsPipelineCreateInfo pipeline_info{};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = static_cast<u32>(shader_stages.size());
    pipeline_info.pStages = shader_stages.data();
    pipeline_info.pVertexInputState = &vertex_input_info;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rasterization;
    pipeline_info.pMultisampleState = &multisample;
    pipeline_info.pDepthStencilState = &depth_stencil;
    pipeline_info.pColorBlendState = &color_blend;
    pipeline_info.pDynamicState = dynamic_states.empty() ? nullptr : &dynamic_state;
    pipeline_info.layout = m_layout->layout();
    pipeline_info.renderPass =
        static_cast<const VulkanRenderPass*>(desc.render_pass)->render_pass();
    pipeline_info.subpass = desc.subpass;

    VkResult result = vkCreateGraphicsPipelines(m_device.device(), VK_NULL_HANDLE, 1,
                                                &pipeline_info, nullptr, &m_pipeline);
    if (result != VK_SUCCESS) {
        HZ_LOG_ERROR("Failed to create Vulkan graphics pipeline: {}", vk_result_string(result));
        return;
    }

    if (desc.debug_name) {
        m_device.set_debug_name(reinterpret_cast<u64>(m_pipeline), desc.debug_name);
    }
}

// ============================================================================
// VulkanPipeline Implementation - Compute
// ============================================================================

VulkanPipeline::VulkanPipeline(VulkanDevice& device, const ComputePipelineDesc& desc)
    : m_device(device)
    , m_layout(static_cast<const VulkanPipelineLayout*>(desc.layout))
    , m_is_compute(true) {

    const auto* vk_shader = static_cast<const VulkanShaderModule*>(desc.compute_shader);

    VkPipelineShaderStageCreateInfo stage_info{};
    stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stage_info.module = vk_shader->module();
    stage_info.pName = vk_shader->entry_point();

    VkComputePipelineCreateInfo pipeline_info{};
    pipeline_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipeline_info.stage = stage_info;
    pipeline_info.layout = m_layout->layout();

    VkResult result = vkCreateComputePipelines(m_device.device(), VK_NULL_HANDLE, 1, &pipeline_info,
                                               nullptr, &m_pipeline);
    if (result != VK_SUCCESS) {
        HZ_LOG_ERROR("Failed to create Vulkan compute pipeline: {}", vk_result_string(result));
        return;
    }

    if (desc.debug_name) {
        m_device.set_debug_name(reinterpret_cast<u64>(m_pipeline), desc.debug_name);
    }
}

VulkanPipeline::~VulkanPipeline() {
    if (m_pipeline != VK_NULL_HANDLE) {
        m_device.defer_deletion(m_pipeline);
        m_pipeline = VK_NULL_HANDLE;
    }
}

} // namespace hz::rhi::vk
