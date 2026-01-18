#pragma once

/**
 * @file rhi.hpp
 * @brief Main include header for the Render Hardware Interface
 *
 * This header includes all RHI components. Include this single header
 * to get access to the complete RHI abstraction layer.
 *
 * @section rhi_overview Overview
 *
 * The RHI (Render Hardware Interface) provides a unified abstraction over
 * modern graphics APIs:
 * - Vulkan 1.2+ (primary target, cross-platform)
 * - DirectX 12 (Windows)
 * - OpenGL 4.5+ (fallback for older hardware)
 *
 * @section rhi_design Design Philosophy
 *
 * The RHI follows these principles:
 * 1. **Thin Abstraction**: Close to Vulkan/DX12 concepts (explicit, low-level)
 * 2. **Command Recording**: All rendering is recorded to command lists
 * 3. **Explicit Synchronization**: Barriers and fences are explicit
 * 4. **Immutable Pipeline State**: PSOs are created upfront
 * 5. **Descriptor-Based Binding**: Resources bound via descriptor sets
 *
 * @section rhi_usage Basic Usage
 *
 * @code
 * #include <engine/rhi/rhi.hpp>
 *
 * using namespace hz::rhi;
 *
 * // Create device
 * auto device = Device::create({
 *     .preferred_backend = Backend::Vulkan,
 *     .enable_validation = true
 * });
 *
 * // Create resources
 * auto vertex_buffer = device->create_vertex_buffer(vertices);
 * auto index_buffer = device->create_index_buffer(indices);
 * auto uniform_buffer = device->create_uniform_buffer(sizeof(CameraData));
 *
 * // Create pipeline
 * auto vertex_shader = device->create_shader_module(vs_bytecode, ShaderStage::Vertex);
 * auto fragment_shader = device->create_shader_module(fs_bytecode, ShaderStage::Fragment);
 *
 * GraphicsPipelineDesc pipeline_desc;
 * pipeline_desc.vertex_shader = vertex_shader.get();
 * pipeline_desc.fragment_shader = fragment_shader.get();
 * pipeline_desc.vertex_layout = VertexInputLayout::standard_vertex();
 * pipeline_desc.layout = pipeline_layout.get();
 * pipeline_desc.render_pass = render_pass.get();
 *
 * auto pipeline = device->create_graphics_pipeline(pipeline_desc);
 *
 * // Render loop
 * while (running) {
 *     u32 frame_index = device->begin_frame();
 *
 *     auto cmd = device->create_command_list();
 *     cmd->begin();
 *
 *     cmd->begin_render_pass(*framebuffer, clear_values);
 *     cmd->bind_pipeline(*pipeline);
 *     cmd->bind_descriptor_set(*layout, 0, *descriptor_set);
 *     cmd->bind_vertex_buffer(0, *vertex_buffer);
 *     cmd->bind_index_buffer(*index_buffer);
 *     cmd->draw_indexed(index_count);
 *     cmd->end_render_pass();
 *
 *     cmd->end();
 *     device->submit(*cmd);
 *
 *     swapchain->present();
 *     device->end_frame();
 * }
 *
 * device->wait_idle();
 * @endcode
 *
 * @section rhi_resources Resource Types
 *
 * | Type              | Description                                      |
 * |-------------------|--------------------------------------------------|
 * | Buffer            | Linear GPU memory (vertices, indices, uniforms)  |
 * | Texture           | Image data (2D, 3D, cube, arrays)                |
 * | TextureView       | View into texture (subset of mips/layers)        |
 * | Sampler           | Texture sampling configuration                   |
 * | ShaderModule      | Compiled shader bytecode                         |
 * | Pipeline          | Complete graphics or compute pipeline state      |
 * | PipelineLayout    | Descriptor set + push constant layout            |
 * | RenderPass        | Attachment configuration for rendering           |
 * | Framebuffer       | Render target collection                         |
 * | DescriptorSetLayout| Template for descriptor set bindings            |
 * | DescriptorSet     | Bound resource collection                        |
 * | Swapchain         | Window presentation                              |
 * | Fence             | CPU-GPU synchronization                          |
 * | Semaphore         | GPU-GPU synchronization                          |
 * | CommandList       | Recorded GPU commands                            |
 *
 * @section rhi_threading Threading Model
 *
 * - Device creation/destruction: Main thread only
 * - Resource creation: Thread-safe (internally synchronized)
 * - Command list recording: One command list per thread
 * - Queue submission: Thread-safe (internally synchronized)
 * - Resource destruction: Thread-safe (deferred deletion)
 */

// Core types and enums
#include "rhi_types.hpp"

// Resource interfaces
#include "rhi_resources.hpp"

// Pipeline and render pass
#include "rhi_pipeline.hpp"

// Descriptor sets
#include "rhi_descriptor.hpp"

// Command recording
#include "rhi_command_list.hpp"

// Device (main entry point)
#include "rhi_device.hpp"

namespace hz::rhi {

/**
 * @brief Get a human-readable name for a backend
 */
[[nodiscard]] constexpr const char* backend_name(Backend backend) noexcept {
    switch (backend) {
    case Backend::Vulkan:
        return "Vulkan";
    case Backend::D3D12:
        return "DirectX 12";
    case Backend::OpenGL:
        return "OpenGL";
    case Backend::Auto:
        return "Auto";
    }
    return "Unknown";
}

/**
 * @brief Get a human-readable name for a device type
 */
[[nodiscard]] constexpr const char* device_type_name(DeviceType type) noexcept {
    switch (type) {
    case DeviceType::DiscreteGPU:
        return "Discrete GPU";
    case DeviceType::IntegratedGPU:
        return "Integrated GPU";
    case DeviceType::VirtualGPU:
        return "Virtual GPU";
    case DeviceType::CPU:
        return "CPU";
    case DeviceType::Other:
        return "Other";
    }
    return "Unknown";
}

/**
 * @brief Get a human-readable name for a vendor
 */
[[nodiscard]] constexpr const char* vendor_name(Vendor vendor) noexcept {
    switch (vendor) {
    case Vendor::AMD:
        return "AMD";
    case Vendor::NVIDIA:
        return "NVIDIA";
    case Vendor::Intel:
        return "Intel";
    case Vendor::ARM:
        return "ARM";
    case Vendor::Qualcomm:
        return "Qualcomm";
    case Vendor::Apple:
        return "Apple";
    case Vendor::Microsoft:
        return "Microsoft (WARP)";
    case Vendor::Unknown:
        return "Unknown";
    }
    return "Unknown";
}

/**
 * @brief Get a human-readable name for a format
 */
[[nodiscard]] constexpr const char* format_name(Format format) noexcept {
    switch (format) {
    case Format::Unknown:
        return "Unknown";
    case Format::R8_UNORM:
        return "R8_UNORM";
    case Format::R8_SNORM:
        return "R8_SNORM";
    case Format::R8_UINT:
        return "R8_UINT";
    case Format::R8_SINT:
        return "R8_SINT";
    case Format::RG8_UNORM:
        return "RG8_UNORM";
    case Format::RG8_SNORM:
        return "RG8_SNORM";
    case Format::RG8_UINT:
        return "RG8_UINT";
    case Format::RG8_SINT:
        return "RG8_SINT";
    case Format::RGBA8_UNORM:
        return "RGBA8_UNORM";
    case Format::RGBA8_SNORM:
        return "RGBA8_SNORM";
    case Format::RGBA8_UINT:
        return "RGBA8_UINT";
    case Format::RGBA8_SINT:
        return "RGBA8_SINT";
    case Format::RGBA8_SRGB:
        return "RGBA8_SRGB";
    case Format::BGRA8_UNORM:
        return "BGRA8_UNORM";
    case Format::BGRA8_SRGB:
        return "BGRA8_SRGB";
    case Format::R16_UNORM:
        return "R16_UNORM";
    case Format::R16_SNORM:
        return "R16_SNORM";
    case Format::R16_UINT:
        return "R16_UINT";
    case Format::R16_SINT:
        return "R16_SINT";
    case Format::R16_FLOAT:
        return "R16_FLOAT";
    case Format::RG16_UNORM:
        return "RG16_UNORM";
    case Format::RG16_SNORM:
        return "RG16_SNORM";
    case Format::RG16_UINT:
        return "RG16_UINT";
    case Format::RG16_SINT:
        return "RG16_SINT";
    case Format::RG16_FLOAT:
        return "RG16_FLOAT";
    case Format::RGBA16_UNORM:
        return "RGBA16_UNORM";
    case Format::RGBA16_SNORM:
        return "RGBA16_SNORM";
    case Format::RGBA16_UINT:
        return "RGBA16_UINT";
    case Format::RGBA16_SINT:
        return "RGBA16_SINT";
    case Format::RGBA16_FLOAT:
        return "RGBA16_FLOAT";
    case Format::R32_UINT:
        return "R32_UINT";
    case Format::R32_SINT:
        return "R32_SINT";
    case Format::R32_FLOAT:
        return "R32_FLOAT";
    case Format::RG32_UINT:
        return "RG32_UINT";
    case Format::RG32_SINT:
        return "RG32_SINT";
    case Format::RG32_FLOAT:
        return "RG32_FLOAT";
    case Format::RGB32_UINT:
        return "RGB32_UINT";
    case Format::RGB32_SINT:
        return "RGB32_SINT";
    case Format::RGB32_FLOAT:
        return "RGB32_FLOAT";
    case Format::RGBA32_UINT:
        return "RGBA32_UINT";
    case Format::RGBA32_SINT:
        return "RGBA32_SINT";
    case Format::RGBA32_FLOAT:
        return "RGBA32_FLOAT";
    case Format::R10G10B10A2_UNORM:
        return "R10G10B10A2_UNORM";
    case Format::R10G10B10A2_UINT:
        return "R10G10B10A2_UINT";
    case Format::R11G11B10_FLOAT:
        return "R11G11B10_FLOAT";
    case Format::D16_UNORM:
        return "D16_UNORM";
    case Format::D24_UNORM_S8_UINT:
        return "D24_UNORM_S8_UINT";
    case Format::D32_FLOAT:
        return "D32_FLOAT";
    case Format::D32_FLOAT_S8_UINT:
        return "D32_FLOAT_S8_UINT";
    case Format::BC1_UNORM:
        return "BC1_UNORM";
    case Format::BC1_SRGB:
        return "BC1_SRGB";
    case Format::BC2_UNORM:
        return "BC2_UNORM";
    case Format::BC2_SRGB:
        return "BC2_SRGB";
    case Format::BC3_UNORM:
        return "BC3_UNORM";
    case Format::BC3_SRGB:
        return "BC3_SRGB";
    case Format::BC4_UNORM:
        return "BC4_UNORM";
    case Format::BC4_SNORM:
        return "BC4_SNORM";
    case Format::BC5_UNORM:
        return "BC5_UNORM";
    case Format::BC5_SNORM:
        return "BC5_SNORM";
    case Format::BC6H_UFLOAT:
        return "BC6H_UFLOAT";
    case Format::BC6H_SFLOAT:
        return "BC6H_SFLOAT";
    case Format::BC7_UNORM:
        return "BC7_UNORM";
    case Format::BC7_SRGB:
        return "BC7_SRGB";
    default:
        return "Unknown";
    }
}

} // namespace hz::rhi
