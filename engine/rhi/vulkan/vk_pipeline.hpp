#pragma once

/**
 * @file vk_pipeline.hpp
 * @brief Vulkan Pipeline, RenderPass, and related implementations
 *
 * Implements the RHI Pipeline interfaces for Vulkan.
 */

#include "../rhi_pipeline.hpp"
#include "vk_common.hpp"

namespace hz::rhi::vk {

class VulkanDevice;
class VulkanDescriptorSetLayout;

// ============================================================================
// Vulkan Shader Module
// ============================================================================

/**
 * @brief Vulkan implementation of the ShaderModule interface
 */
class VulkanShaderModule final : public ShaderModule {
public:
    VulkanShaderModule(VulkanDevice& device, const ShaderModuleDesc& desc);
    ~VulkanShaderModule() override;

    [[nodiscard]] ShaderStage stage() const noexcept override { return m_stage; }
    [[nodiscard]] const char* entry_point() const noexcept override {
        return m_entry_point.c_str();
    }

    [[nodiscard]] u64 native_handle() const noexcept override {
        return reinterpret_cast<u64>(m_module);
    }

    [[nodiscard]] VkShaderModule module() const noexcept { return m_module; }

private:
    VulkanDevice& m_device;
    VkShaderModule m_module{VK_NULL_HANDLE};
    ShaderStage m_stage{ShaderStage::None};
    std::string m_entry_point;
};

// ============================================================================
// Vulkan Render Pass
// ============================================================================

/**
 * @brief Vulkan implementation of the RenderPass interface
 */
class VulkanRenderPass final : public RenderPass {
public:
    VulkanRenderPass(VulkanDevice& device, const RenderPassDesc& desc);
    ~VulkanRenderPass() override;

    [[nodiscard]] u32 color_attachment_count() const noexcept override {
        return static_cast<u32>(m_color_formats.size());
    }
    [[nodiscard]] bool has_depth_stencil() const noexcept override { return m_has_depth_stencil; }
    [[nodiscard]] Format color_format(u32 index) const noexcept override {
        return index < m_color_formats.size() ? m_color_formats[index] : Format::Unknown;
    }
    [[nodiscard]] Format depth_stencil_format() const noexcept override {
        return m_depth_stencil_format;
    }

    [[nodiscard]] u64 native_handle() const noexcept override {
        return reinterpret_cast<u64>(m_render_pass);
    }

    [[nodiscard]] VkRenderPass render_pass() const noexcept { return m_render_pass; }

private:
    VulkanDevice& m_device;
    VkRenderPass m_render_pass{VK_NULL_HANDLE};

    std::vector<Format> m_color_formats;
    Format m_depth_stencil_format{Format::Unknown};
    bool m_has_depth_stencil{false};
};

// ============================================================================
// Vulkan Framebuffer
// ============================================================================

/**
 * @brief Vulkan implementation of the Framebuffer interface
 */
class VulkanFramebuffer final : public Framebuffer {
public:
    VulkanFramebuffer(VulkanDevice& device, const FramebufferDesc& desc);
    ~VulkanFramebuffer() override;

    [[nodiscard]] u32 width() const noexcept override { return m_width; }
    [[nodiscard]] u32 height() const noexcept override { return m_height; }
    [[nodiscard]] u32 layers() const noexcept override { return m_layers; }
    [[nodiscard]] const RenderPass& render_pass() const noexcept override { return *m_render_pass; }

    [[nodiscard]] u64 native_handle() const noexcept override {
        return reinterpret_cast<u64>(m_framebuffer);
    }

    [[nodiscard]] VkFramebuffer framebuffer() const noexcept { return m_framebuffer; }

private:
    VulkanDevice& m_device;
    VkFramebuffer m_framebuffer{VK_NULL_HANDLE};
    const VulkanRenderPass* m_render_pass{nullptr};

    u32 m_width{0};
    u32 m_height{0};
    u32 m_layers{1};
};

// ============================================================================
// Vulkan Pipeline Layout
// ============================================================================

/**
 * @brief Vulkan implementation of the PipelineLayout interface
 */
class VulkanPipelineLayout final : public PipelineLayout {
public:
    VulkanPipelineLayout(VulkanDevice& device, const PipelineLayoutDesc& desc);
    ~VulkanPipelineLayout() override;

    [[nodiscard]] u32 descriptor_set_count() const noexcept override {
        return m_descriptor_set_count;
    }
    [[nodiscard]] u32 push_constant_size() const noexcept override { return m_push_constant_size; }

    [[nodiscard]] u64 native_handle() const noexcept override {
        return reinterpret_cast<u64>(m_layout);
    }

    [[nodiscard]] VkPipelineLayout layout() const noexcept { return m_layout; }

private:
    VulkanDevice& m_device;
    VkPipelineLayout m_layout{VK_NULL_HANDLE};
    u32 m_descriptor_set_count{0};
    u32 m_push_constant_size{0};
};

// ============================================================================
// Vulkan Pipeline
// ============================================================================

/**
 * @brief Vulkan implementation of the Pipeline interface
 */
class VulkanPipeline final : public Pipeline {
public:
    VulkanPipeline(VulkanDevice& device, const GraphicsPipelineDesc& desc);
    VulkanPipeline(VulkanDevice& device, const ComputePipelineDesc& desc);
    ~VulkanPipeline() override;

    [[nodiscard]] bool is_compute() const noexcept override { return m_is_compute; }
    [[nodiscard]] const PipelineLayout& layout() const noexcept override { return *m_layout; }

    [[nodiscard]] u64 native_handle() const noexcept override {
        return reinterpret_cast<u64>(m_pipeline);
    }

    [[nodiscard]] VkPipeline pipeline() const noexcept { return m_pipeline; }
    [[nodiscard]] VkPipelineBindPoint bind_point() const noexcept {
        return m_is_compute ? VK_PIPELINE_BIND_POINT_COMPUTE : VK_PIPELINE_BIND_POINT_GRAPHICS;
    }

private:
    VulkanDevice& m_device;
    VkPipeline m_pipeline{VK_NULL_HANDLE};
    const VulkanPipelineLayout* m_layout{nullptr};
    bool m_is_compute{false};
};

} // namespace hz::rhi::vk
