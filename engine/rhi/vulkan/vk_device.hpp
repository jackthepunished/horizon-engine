#pragma once

/**
 * @file vk_device.hpp
 * @brief Vulkan Device implementation
 *
 * Implements the RHI Device interface for Vulkan, managing:
 * - VkInstance creation with validation layers
 * - Physical device selection
 * - Logical device and queue creation
 * - VMA allocator
 * - Resource creation
 */

#include "../rhi_device.hpp"
#include "vk_common.hpp"

#include <array>
#include <vector>

namespace hz::rhi::vk {

// Forward declarations
class VulkanBuffer;
class VulkanTexture;
class VulkanTextureView;
class VulkanSampler;
class VulkanShaderModule;
class VulkanRenderPass;
class VulkanFramebuffer;
class VulkanPipelineLayout;
class VulkanPipeline;
class VulkanDescriptorSetLayout;
class VulkanDescriptorPool;
class VulkanDescriptorSet;
class VulkanCommandList;
class VulkanSwapchain;
class VulkanFence;
class VulkanSemaphore;

// ============================================================================
// Queue Family Indices
// ============================================================================

struct QueueFamilyIndices {
    u32 graphics{UINT32_MAX};
    u32 compute{UINT32_MAX};
    u32 transfer{UINT32_MAX};
    u32 present{UINT32_MAX};

    [[nodiscard]] bool is_complete() const noexcept { return graphics != UINT32_MAX; }
};

// ============================================================================
// Frame Data
// ============================================================================

/**
 * @brief Per-frame resources for double/triple buffering
 */
struct FrameData {
    VkCommandPool command_pool{VK_NULL_HANDLE};
    VkFence in_flight_fence{VK_NULL_HANDLE};
    VkSemaphore image_available_semaphore{VK_NULL_HANDLE};
    VkSemaphore render_finished_semaphore{VK_NULL_HANDLE};

    // Deletion queues for deferred resource cleanup
    std::vector<VkBuffer> buffers_to_delete;
    std::vector<VmaAllocation> allocations_to_delete;
    std::vector<VkImage> images_to_delete;
    std::vector<VkImageView> image_views_to_delete;
    std::vector<VkSampler> samplers_to_delete;
    std::vector<VkPipeline> pipelines_to_delete;
    std::vector<VkPipelineLayout> pipeline_layouts_to_delete;
    std::vector<VkRenderPass> render_passes_to_delete;
    std::vector<VkFramebuffer> framebuffers_to_delete;
    std::vector<VkDescriptorSetLayout> descriptor_set_layouts_to_delete;
    std::vector<VkDescriptorPool> descriptor_pools_to_delete;
    std::vector<VkShaderModule> shader_modules_to_delete;
};

// ============================================================================
// Vulkan Device
// ============================================================================

/**
 * @brief Vulkan implementation of the RHI Device interface
 */
class VulkanDevice final : public Device {
public:
    explicit VulkanDevice(const DeviceDesc& desc);
    ~VulkanDevice() override;

    // ========================================================================
    // Device Interface
    // ========================================================================

    [[nodiscard]] Backend backend() const noexcept override { return Backend::Vulkan; }
    [[nodiscard]] const DeviceInfo& device_info() const noexcept override { return m_device_info; }
    [[nodiscard]] const DeviceLimits& limits() const noexcept override { return m_limits; }

    // Resource creation
    [[nodiscard]] std::unique_ptr<Buffer> create_buffer(const BufferDesc& desc) override;
    [[nodiscard]] std::unique_ptr<Texture> create_texture(const TextureDesc& desc) override;
    [[nodiscard]] std::unique_ptr<TextureView>
    create_texture_view(const TextureViewDesc& desc) override;
    [[nodiscard]] std::unique_ptr<Sampler> create_sampler(const SamplerDesc& desc) override;
    [[nodiscard]] std::unique_ptr<ShaderModule>
    create_shader_module(const ShaderModuleDesc& desc) override;
    [[nodiscard]] std::unique_ptr<RenderPass>
    create_render_pass(const RenderPassDesc& desc) override;
    [[nodiscard]] std::unique_ptr<Framebuffer>
    create_framebuffer(const FramebufferDesc& desc) override;
    [[nodiscard]] std::unique_ptr<PipelineLayout>
    create_pipeline_layout(const PipelineLayoutDesc& desc) override;
    [[nodiscard]] std::unique_ptr<Pipeline>
    create_graphics_pipeline(const GraphicsPipelineDesc& desc) override;
    [[nodiscard]] std::unique_ptr<Pipeline>
    create_compute_pipeline(const ComputePipelineDesc& desc) override;
    [[nodiscard]] std::unique_ptr<DescriptorSetLayout>
    create_descriptor_set_layout(const DescriptorSetLayoutDesc& desc) override;
    [[nodiscard]] std::unique_ptr<DescriptorPool>
    create_descriptor_pool(const DescriptorPoolDesc& desc) override;
    [[nodiscard]] std::unique_ptr<Fence> create_fence(bool signaled) override;
    [[nodiscard]] std::unique_ptr<Semaphore> create_semaphore() override;
    [[nodiscard]] std::unique_ptr<Swapchain> create_swapchain(const SwapchainDesc& desc) override;
    [[nodiscard]] std::unique_ptr<CommandList> create_command_list(QueueType queue_type) override;

    // Submission
    void submit(QueueType queue_type, std::span<const SubmitInfo> submits) override;

    // Synchronization
    void wait_queue_idle(QueueType queue_type) override;
    void wait_idle() override;
    [[nodiscard]] bool wait_fences(std::span<Fence* const> fences, bool wait_all,
                                   u64 timeout_ns) override;
    void reset_fences(std::span<Fence* const> fences) override;

    // Frame management
    [[nodiscard]] u32 begin_frame() override;
    void end_frame() override;
    [[nodiscard]] u32 current_frame_index() const noexcept override { return m_current_frame; }
    [[nodiscard]] u32 frame_buffer_count() const noexcept override { return MAX_FRAMES_IN_FLIGHT; }

    // Utility
    void update_buffer(Buffer& buffer, const void* data, u64 size, u64 offset) override;
    void update_texture(Texture& texture, const void* data, u64 size, u32 mip_level,
                        u32 array_layer, Offset3D offset) override;
    void generate_mipmaps(Texture& texture) override;
    void set_debug_name(u64 handle, const char* name) override;

    // Native handles
    [[nodiscard]] u64 native_handle() const noexcept override {
        return reinterpret_cast<u64>(m_device);
    }
    [[nodiscard]] u64 native_instance() const noexcept override {
        return reinterpret_cast<u64>(m_instance);
    }
    [[nodiscard]] u64 native_physical_device() const noexcept override {
        return reinterpret_cast<u64>(m_physical_device);
    }

    // ========================================================================
    // Vulkan-Specific Accessors
    // ========================================================================

    [[nodiscard]] VkInstance instance() const noexcept { return m_instance; }
    [[nodiscard]] VkPhysicalDevice physical_device() const noexcept { return m_physical_device; }
    [[nodiscard]] VkDevice device() const noexcept { return m_device; }
    [[nodiscard]] VmaAllocator allocator() const noexcept { return m_allocator; }

    [[nodiscard]] VkQueue graphics_queue() const noexcept { return m_graphics_queue; }
    [[nodiscard]] VkQueue compute_queue() const noexcept { return m_compute_queue; }
    [[nodiscard]] VkQueue transfer_queue() const noexcept { return m_transfer_queue; }

    [[nodiscard]] const QueueFamilyIndices& queue_families() const noexcept {
        return m_queue_families;
    }

    [[nodiscard]] VkQueue get_queue(QueueType type) const noexcept {
        switch (type) {
        case QueueType::Graphics:
            return m_graphics_queue;
        case QueueType::Compute:
            return m_compute_queue;
        case QueueType::Transfer:
            return m_transfer_queue;
        }
        return m_graphics_queue;
    }

    [[nodiscard]] u32 get_queue_family(QueueType type) const noexcept {
        switch (type) {
        case QueueType::Graphics:
            return m_queue_families.graphics;
        case QueueType::Compute:
            return m_queue_families.compute;
        case QueueType::Transfer:
            return m_queue_families.transfer;
        }
        return m_queue_families.graphics;
    }

    [[nodiscard]] FrameData& current_frame_data() noexcept { return m_frames[m_current_frame]; }

    // ========================================================================
    // Deferred Deletion
    // ========================================================================

    void defer_deletion(VkBuffer buffer, VmaAllocation allocation);
    void defer_deletion(VkImage image, VmaAllocation allocation);
    void defer_deletion(VkImageView view);
    void defer_deletion(VkSampler sampler);
    void defer_deletion(VkPipeline pipeline);
    void defer_deletion(VkPipelineLayout layout);
    void defer_deletion(VkRenderPass render_pass);
    void defer_deletion(VkFramebuffer framebuffer);
    void defer_deletion(VkDescriptorSetLayout layout);
    void defer_deletion(VkDescriptorPool pool);
    void defer_deletion(VkShaderModule module);

private:
    static constexpr u32 MAX_FRAMES_IN_FLIGHT = 2;

    // ========================================================================
    // Initialization
    // ========================================================================

    bool create_instance(const DeviceDesc& desc);
    bool setup_debug_messenger();
    bool select_physical_device(bool prefer_discrete);
    bool create_logical_device();
    bool create_allocator();
    bool create_frame_resources();

    void populate_device_info();
    void populate_device_limits();

    // ========================================================================
    // Cleanup
    // ========================================================================

    void destroy_frame_resources();
    void process_deletion_queue(FrameData& frame);

    // ========================================================================
    // Helpers
    // ========================================================================

    [[nodiscard]] QueueFamilyIndices find_queue_families(VkPhysicalDevice device) const;
    [[nodiscard]] bool check_device_extension_support(VkPhysicalDevice device) const;
    [[nodiscard]] int rate_device_suitability(VkPhysicalDevice device) const;

    // ========================================================================
    // Members
    // ========================================================================

    // Core Vulkan objects
    VkInstance m_instance{VK_NULL_HANDLE};
    VkDebugUtilsMessengerEXT m_debug_messenger{VK_NULL_HANDLE};
    VkPhysicalDevice m_physical_device{VK_NULL_HANDLE};
    VkDevice m_device{VK_NULL_HANDLE};
    VmaAllocator m_allocator{VK_NULL_HANDLE};

    // Queues
    QueueFamilyIndices m_queue_families;
    VkQueue m_graphics_queue{VK_NULL_HANDLE};
    VkQueue m_compute_queue{VK_NULL_HANDLE};
    VkQueue m_transfer_queue{VK_NULL_HANDLE};

    // Device info
    DeviceInfo m_device_info{};
    DeviceLimits m_limits{};

    // Frame management
    std::array<FrameData, MAX_FRAMES_IN_FLIGHT> m_frames{};
    u32 m_current_frame{0};
    u64 m_frame_number{0};

    // Configuration
    bool m_validation_enabled{false};
    std::function<void(const char*, bool)> m_debug_callback;

    // Required device extensions
    static constexpr std::array REQUIRED_DEVICE_EXTENSIONS = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
    };
};

} // namespace hz::rhi::vk
