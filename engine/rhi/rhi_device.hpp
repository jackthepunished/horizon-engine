#pragma once

/**
 * @file rhi_device.hpp
 * @brief RHI Device interface - the main entry point for creating GPU resources
 *
 * The Device is the central factory for all RHI resources. It represents a logical
 * connection to a physical GPU and provides methods to create resources, command lists,
 * and submit work to the GPU.
 */

#include "rhi_command_list.hpp"
#include "rhi_descriptor.hpp"
#include "rhi_pipeline.hpp"
#include "rhi_resources.hpp"
#include "rhi_types.hpp"

#include <functional>
#include <memory>
#include <span>

namespace hz::rhi {

// ============================================================================
// Device Creation
// ============================================================================

/**
 * @brief Configuration for creating an RHI device
 */
struct DeviceDesc {
    Backend preferred_backend{Backend::Auto}; ///< Preferred graphics API
    bool enable_validation{true};             ///< Enable API validation layers
    bool enable_gpu_validation{false};        ///< Enable GPU-assisted validation (slow)
    bool enable_debug_markers{true};          ///< Enable debug markers for profilers
    bool prefer_discrete_gpu{true};           ///< Prefer discrete over integrated GPU
    const char* application_name{"Horizon Engine"};
    u32 application_version{1};

    /// Optional callback for validation/debug messages
    std::function<void(const char* message, bool is_error)> debug_callback;
};

// ============================================================================
// Queue Submission
// ============================================================================

/**
 * @brief Describes work to submit to a queue
 */
struct SubmitInfo {
    std::span<CommandList* const> command_lists;
    std::span<Semaphore* const> wait_semaphores;   ///< Semaphores to wait on before execution
    std::span<Semaphore* const> signal_semaphores; ///< Semaphores to signal after completion
    Fence* signal_fence{nullptr};                  ///< Fence to signal after completion
};

// ============================================================================
// Device Interface
// ============================================================================

/**
 * @brief Abstract RHI device interface
 *
 * The Device is the main entry point for all RHI operations. It serves as:
 * - A factory for creating GPU resources (buffers, textures, pipelines, etc.)
 * - A submission point for GPU work
 * - A synchronization manager for CPU-GPU coordination
 *
 * Usage:
 * @code
 * auto device = Device::create({.preferred_backend = Backend::Vulkan});
 *
 * auto buffer = device->create_buffer({.size = 1024, .usage = BufferUsage::VertexBuffer});
 * auto cmd = device->create_command_list(QueueType::Graphics);
 *
 * cmd->begin();
 * // ... record commands ...
 * cmd->end();
 *
 * device->submit(QueueType::Graphics, {.command_lists = {&cmd, 1}});
 * @endcode
 */
class Device {
public:
    virtual ~Device() = default;

    // ========================================================================
    // Factory Method
    // ========================================================================

    /**
     * @brief Create an RHI device
     * @param desc Device configuration
     * @return Unique pointer to device, or nullptr on failure
     */
    [[nodiscard]] static std::unique_ptr<Device> create(const DeviceDesc& desc = {});

    // ========================================================================
    // Device Information
    // ========================================================================

    /**
     * @brief Get the active graphics backend
     */
    [[nodiscard]] virtual Backend backend() const noexcept = 0;

    /**
     * @brief Get information about the physical device
     */
    [[nodiscard]] virtual const DeviceInfo& device_info() const noexcept = 0;

    /**
     * @brief Get device capability limits
     */
    [[nodiscard]] virtual const DeviceLimits& limits() const noexcept = 0;

    // ========================================================================
    // Resource Creation - Buffers
    // ========================================================================

    /**
     * @brief Create a GPU buffer
     */
    [[nodiscard]] virtual std::unique_ptr<Buffer> create_buffer(const BufferDesc& desc) = 0;

    /**
     * @brief Create a buffer with initial data
     */
    [[nodiscard]] std::unique_ptr<Buffer> create_buffer(u64 size, BufferUsage usage,
                                                        const void* initial_data = nullptr,
                                                        MemoryUsage memory = MemoryUsage::GPU_Only,
                                                        const char* debug_name = nullptr) {
        return create_buffer({size, usage, memory, initial_data, debug_name});
    }

    /**
     * @brief Create a vertex buffer with data
     */
    template <typename T>
    [[nodiscard]] std::unique_ptr<Buffer> create_vertex_buffer(std::span<const T> vertices,
                                                               const char* debug_name = nullptr) {
        return create_buffer({.size = vertices.size_bytes(),
                              .usage = BufferUsage::VertexBuffer | BufferUsage::TransferDst,
                              .memory = MemoryUsage::GPU_Only,
                              .initial_data = vertices.data(),
                              .debug_name = debug_name});
    }

    /**
     * @brief Create an index buffer with data
     */
    template <typename T>
    [[nodiscard]] std::unique_ptr<Buffer> create_index_buffer(std::span<const T> indices,
                                                              const char* debug_name = nullptr) {
        static_assert(std::is_same_v<T, u16> || std::is_same_v<T, u32>,
                      "Index type must be u16 or u32");
        return create_buffer({.size = indices.size_bytes(),
                              .usage = BufferUsage::IndexBuffer | BufferUsage::TransferDst,
                              .memory = MemoryUsage::GPU_Only,
                              .initial_data = indices.data(),
                              .debug_name = debug_name});
    }

    /**
     * @brief Create a uniform buffer
     */
    [[nodiscard]] std::unique_ptr<Buffer> create_uniform_buffer(u64 size,
                                                                const char* debug_name = nullptr) {
        return create_buffer({.size = size,
                              .usage = BufferUsage::UniformBuffer,
                              .memory = MemoryUsage::CPU_To_GPU,
                              .debug_name = debug_name});
    }

    /**
     * @brief Create a staging buffer for uploads
     */
    [[nodiscard]] std::unique_ptr<Buffer> create_staging_buffer(u64 size,
                                                                const char* debug_name = nullptr) {
        return create_buffer({.size = size,
                              .usage = BufferUsage::TransferSrc,
                              .memory = MemoryUsage::CPU_To_GPU,
                              .debug_name = debug_name});
    }

    // ========================================================================
    // Resource Creation - Textures
    // ========================================================================

    /**
     * @brief Create a texture
     */
    [[nodiscard]] virtual std::unique_ptr<Texture> create_texture(const TextureDesc& desc) = 0;

    /**
     * @brief Create a texture view
     */
    [[nodiscard]] virtual std::unique_ptr<TextureView>
    create_texture_view(const TextureViewDesc& desc) = 0;

    /**
     * @brief Create a default view for a texture (full mip chain, all layers)
     */
    [[nodiscard]] std::unique_ptr<TextureView>
    create_texture_view(const Texture& texture, const char* debug_name = nullptr) {
        TextureViewDesc desc;
        desc.texture = &texture;
        desc.view_type = texture.type();
        desc.format = texture.format();
        desc.mip_level_count = texture.mip_levels();
        desc.array_layer_count = texture.array_layers();
        desc.debug_name = debug_name;
        return create_texture_view(desc);
    }

    // ========================================================================
    // Resource Creation - Samplers
    // ========================================================================

    /**
     * @brief Create a sampler
     */
    [[nodiscard]] virtual std::unique_ptr<Sampler> create_sampler(const SamplerDesc& desc) = 0;

    // ========================================================================
    // Resource Creation - Shaders
    // ========================================================================

    /**
     * @brief Create a shader module from bytecode
     */
    [[nodiscard]] virtual std::unique_ptr<ShaderModule>
    create_shader_module(const ShaderModuleDesc& desc) = 0;

    /**
     * @brief Convenience: create shader from byte span
     */
    [[nodiscard]] std::unique_ptr<ShaderModule>
    create_shader_module(std::span<const u8> bytecode, ShaderStage stage,
                         const char* entry_point = "main", const char* debug_name = nullptr) {
        return create_shader_module({bytecode, stage, entry_point, debug_name});
    }

    // ========================================================================
    // Resource Creation - Render Pass & Framebuffer
    // ========================================================================

    /**
     * @brief Create a render pass
     */
    [[nodiscard]] virtual std::unique_ptr<RenderPass>
    create_render_pass(const RenderPassDesc& desc) = 0;

    /**
     * @brief Create a framebuffer
     */
    [[nodiscard]] virtual std::unique_ptr<Framebuffer>
    create_framebuffer(const FramebufferDesc& desc) = 0;

    // ========================================================================
    // Resource Creation - Pipelines
    // ========================================================================

    /**
     * @brief Create a pipeline layout
     */
    [[nodiscard]] virtual std::unique_ptr<PipelineLayout>
    create_pipeline_layout(const PipelineLayoutDesc& desc) = 0;

    /**
     * @brief Create a graphics pipeline
     */
    [[nodiscard]] virtual std::unique_ptr<Pipeline>
    create_graphics_pipeline(const GraphicsPipelineDesc& desc) = 0;

    /**
     * @brief Create a compute pipeline
     */
    [[nodiscard]] virtual std::unique_ptr<Pipeline>
    create_compute_pipeline(const ComputePipelineDesc& desc) = 0;

    // ========================================================================
    // Resource Creation - Descriptors
    // ========================================================================

    /**
     * @brief Create a descriptor set layout
     */
    [[nodiscard]] virtual std::unique_ptr<DescriptorSetLayout>
    create_descriptor_set_layout(const DescriptorSetLayoutDesc& desc) = 0;

    /**
     * @brief Create a descriptor pool
     */
    [[nodiscard]] virtual std::unique_ptr<DescriptorPool>
    create_descriptor_pool(const DescriptorPoolDesc& desc) = 0;

    // ========================================================================
    // Resource Creation - Synchronization
    // ========================================================================

    /**
     * @brief Create a fence for CPU-GPU synchronization
     * @param signaled Initial state (true = signaled)
     */
    [[nodiscard]] virtual std::unique_ptr<Fence> create_fence(bool signaled = false) = 0;

    /**
     * @brief Create a semaphore for GPU-GPU synchronization
     */
    [[nodiscard]] virtual std::unique_ptr<Semaphore> create_semaphore() = 0;

    // ========================================================================
    // Resource Creation - Swapchain
    // ========================================================================

    /**
     * @brief Create a swapchain for presenting to a window
     */
    [[nodiscard]] virtual std::unique_ptr<Swapchain>
    create_swapchain(const SwapchainDesc& desc) = 0;

    // ========================================================================
    // Command List Management
    // ========================================================================

    /**
     * @brief Create a command list for the specified queue type
     */
    [[nodiscard]] virtual std::unique_ptr<CommandList>
    create_command_list(QueueType queue_type = QueueType::Graphics) = 0;

    // ========================================================================
    // Queue Submission
    // ========================================================================

    /**
     * @brief Submit work to a queue
     * @param queue_type Which queue to submit to
     * @param submits Work to submit
     */
    virtual void submit(QueueType queue_type, std::span<const SubmitInfo> submits) = 0;

    /**
     * @brief Convenience: submit a single command list
     */
    void submit(CommandList& cmd, Fence* signal_fence = nullptr) {
        CommandList* ptr = &cmd;
        SubmitInfo info;
        info.command_lists = {&ptr, 1};
        info.signal_fence = signal_fence;
        submit(cmd.queue_type(), {&info, 1});
    }

    /**
     * @brief Convenience: submit and signal a fence
     */
    void submit(CommandList& cmd, Fence& fence) { submit(cmd, &fence); }

    // ========================================================================
    // Synchronization
    // ========================================================================

    /**
     * @brief Wait for a queue to become idle
     */
    virtual void wait_queue_idle(QueueType queue_type) = 0;

    /**
     * @brief Wait for all queues to become idle
     */
    virtual void wait_idle() = 0;

    /**
     * @brief Wait for multiple fences
     * @param fences Fences to wait for
     * @param wait_all true = wait for all, false = wait for any
     * @param timeout_ns Timeout in nanoseconds (UINT64_MAX = infinite)
     * @return true if all/any fences signaled, false if timeout
     */
    [[nodiscard]] virtual bool wait_fences(std::span<Fence* const> fences, bool wait_all = true,
                                           u64 timeout_ns = UINT64_MAX) = 0;

    /**
     * @brief Reset multiple fences to unsignaled state
     */
    virtual void reset_fences(std::span<Fence* const> fences) = 0;

    // ========================================================================
    // Frame Management
    // ========================================================================

    /**
     * @brief Begin a new frame
     *
     * Call this at the start of each frame. Handles internal resource recycling
     * and frame synchronization.
     *
     * @return Frame index (wraps around based on buffering)
     */
    [[nodiscard]] virtual u32 begin_frame() = 0;

    /**
     * @brief End the current frame
     *
     * Call this at the end of each frame, after all submissions.
     */
    virtual void end_frame() = 0;

    /**
     * @brief Get the current frame index
     */
    [[nodiscard]] virtual u32 current_frame_index() const noexcept = 0;

    /**
     * @brief Get the number of buffered frames (typically 2 or 3)
     */
    [[nodiscard]] virtual u32 frame_buffer_count() const noexcept = 0;

    // ========================================================================
    // Utility Methods
    // ========================================================================

    /**
     * @brief Update buffer data on GPU
     *
     * For GPU-only buffers, this stages the data and copies it.
     * For CPU-visible buffers, this directly maps and writes.
     *
     * @param buffer Target buffer
     * @param data Source data
     * @param size Size in bytes
     * @param offset Byte offset into buffer
     */
    virtual void update_buffer(Buffer& buffer, const void* data, u64 size, u64 offset = 0) = 0;

    /**
     * @brief Update texture data on GPU
     *
     * @param texture Target texture
     * @param data Source data
     * @param size Size in bytes
     * @param mip_level Mip level to update
     * @param array_layer Array layer to update
     * @param offset Offset within the mip/layer
     */
    virtual void update_texture(Texture& texture, const void* data, u64 size, u32 mip_level = 0,
                                u32 array_layer = 0, Offset3D offset = {}) = 0;

    /**
     * @brief Generate mipmaps for a texture
     *
     * Uses GPU blitting to generate full mipmap chain.
     *
     * @param texture Texture to generate mipmaps for
     */
    virtual void generate_mipmaps(Texture& texture) = 0;

    // ========================================================================
    // Debug
    // ========================================================================

    /**
     * @brief Set a debug name for an object
     *
     * Names are visible in graphics debuggers like RenderDoc, PIX, etc.
     */
    virtual void set_debug_name(u64 handle, const char* name) = 0;

    // ========================================================================
    // Native Handle
    // ========================================================================

    /**
     * @brief Get the backend-specific native handle
     * @return Vulkan: VkDevice, DX12: ID3D12Device*, OpenGL: internal context
     */
    [[nodiscard]] virtual u64 native_handle() const noexcept = 0;

    /**
     * @brief Get the native instance/factory handle
     * @return Vulkan: VkInstance, DX12: IDXGIFactory*, OpenGL: N/A
     */
    [[nodiscard]] virtual u64 native_instance() const noexcept = 0;

    /**
     * @brief Get the native physical device handle
     * @return Vulkan: VkPhysicalDevice, DX12: IDXGIAdapter*, OpenGL: N/A
     */
    [[nodiscard]] virtual u64 native_physical_device() const noexcept = 0;

protected:
    Device() = default;
    HZ_NON_COPYABLE(Device);
    HZ_NON_MOVABLE(Device);
};

// ============================================================================
// Immediate Context Helper
// ============================================================================

/**
 * @brief Helper for executing immediate GPU commands
 *
 * Useful for one-off operations like resource uploads during loading.
 *
 * Usage:
 * @code
 * device->immediate_submit([&](CommandList& cmd) {
 *     cmd.copy_buffer(staging, dest, {{0, 0, size}});
 * });
 * @endcode
 */
class ImmediateContext {
public:
    explicit ImmediateContext(Device& device) : m_device(device) {
        m_cmd = m_device.create_command_list(QueueType::Graphics);
        m_fence = m_device.create_fence(false);
    }

    /**
     * @brief Execute commands immediately and wait for completion
     */
    void submit(std::function<void(CommandList&)> record_fn) {
        m_cmd->reset();
        m_cmd->begin();
        record_fn(*m_cmd);
        m_cmd->end();

        m_device.submit(*m_cmd, *m_fence);
        (void)m_fence->wait();
        m_fence->reset();
    }

private:
    Device& m_device;
    std::unique_ptr<CommandList> m_cmd;
    std::unique_ptr<Fence> m_fence;
};

} // namespace hz::rhi
