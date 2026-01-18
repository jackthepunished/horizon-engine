#pragma once

/**
 * @file vk_swapchain.hpp
 * @brief Vulkan Swapchain implementation
 *
 * Implements the RHI Swapchain interface for Vulkan, managing:
 * - VkSurfaceKHR creation
 * - VkSwapchainKHR management
 * - Image acquisition and presentation
 * - Resize handling
 */

#include "../rhi_resources.hpp"
#include "vk_common.hpp"

#include <vector>

namespace hz::rhi::vk {

class VulkanDevice;
class VulkanTexture;
class VulkanTextureView;
class VulkanSemaphore;

/**
 * @brief Swapchain support details for surface/device combination
 */
struct SwapchainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities{};
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> present_modes;

    [[nodiscard]] bool is_adequate() const noexcept {
        return !formats.empty() && !present_modes.empty();
    }
};

/**
 * @brief Vulkan implementation of the Swapchain interface
 *
 * Manages the Vulkan swapchain, surface, and associated images for presenting
 * rendered frames to a window.
 */
class VulkanSwapchain final : public Swapchain {
public:
    /**
     * @brief Create a swapchain for a window
     * @param device The Vulkan device
     * @param desc Swapchain description
     */
    VulkanSwapchain(VulkanDevice& device, const SwapchainDesc& desc);
    ~VulkanSwapchain() override;

    // ========================================================================
    // Swapchain Interface
    // ========================================================================

    [[nodiscard]] u32 width() const noexcept override { return m_width; }
    [[nodiscard]] u32 height() const noexcept override { return m_height; }
    [[nodiscard]] Format format() const noexcept override { return m_format; }
    [[nodiscard]] u32 image_count() const noexcept override {
        return static_cast<u32>(m_images.size());
    }
    [[nodiscard]] u32 current_image_index() const noexcept override {
        return m_current_image_index;
    }

    [[nodiscard]] Texture* get_current_texture() override;
    [[nodiscard]] TextureView* get_current_view() override;

    [[nodiscard]] bool acquire_next_image(Semaphore* signal_semaphore = nullptr) override;
    void present(std::span<Semaphore* const> wait_semaphores = {}) override;
    void resize(u32 width, u32 height) override;

    // ========================================================================
    // Vulkan-Specific Accessors
    // ========================================================================

    [[nodiscard]] VkSwapchainKHR swapchain() const noexcept { return m_swapchain; }
    [[nodiscard]] VkSurfaceKHR surface() const noexcept { return m_surface; }
    [[nodiscard]] VkFormat vk_format() const noexcept { return m_vk_format; }
    [[nodiscard]] VkColorSpaceKHR color_space() const noexcept { return m_color_space; }
    [[nodiscard]] VkPresentModeKHR present_mode() const noexcept { return m_present_mode; }

    /**
     * @brief Get image at specific index
     */
    [[nodiscard]] VulkanTexture* get_texture(u32 index) const;
    [[nodiscard]] VulkanTextureView* get_view(u32 index) const;

    /**
     * @brief Query swapchain support for a surface/device combination
     */
    [[nodiscard]] static SwapchainSupportDetails query_support(VkPhysicalDevice device,
                                                               VkSurfaceKHR surface);

    /**
     * @brief Check if swapchain needs to be recreated (suboptimal or out of date)
     */
    [[nodiscard]] bool needs_recreation() const noexcept { return m_needs_recreation; }

private:
    // ========================================================================
    // Initialization / Cleanup
    // ========================================================================

    bool create_surface(void* window_handle);
    bool create_swapchain(u32 width, u32 height, bool vsync);
    void create_image_views();
    void cleanup_swapchain();

    // ========================================================================
    // Selection Helpers
    // ========================================================================

    [[nodiscard]] VkSurfaceFormatKHR
    choose_surface_format(const std::vector<VkSurfaceFormatKHR>& available_formats,
                          Format preferred) const;

    [[nodiscard]] VkPresentModeKHR
    choose_present_mode(const std::vector<VkPresentModeKHR>& available_modes, bool vsync) const;

    [[nodiscard]] VkExtent2D choose_extent(const VkSurfaceCapabilitiesKHR& capabilities, u32 width,
                                           u32 height) const;

    // ========================================================================
    // Members
    // ========================================================================

    VulkanDevice& m_device;

    VkSurfaceKHR m_surface{VK_NULL_HANDLE};
    VkSwapchainKHR m_swapchain{VK_NULL_HANDLE};

    // Swapchain properties
    u32 m_width{0};
    u32 m_height{0};
    Format m_format{Format::BGRA8_SRGB};
    VkFormat m_vk_format{VK_FORMAT_B8G8R8A8_SRGB};
    VkColorSpaceKHR m_color_space{VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    VkPresentModeKHR m_present_mode{VK_PRESENT_MODE_FIFO_KHR};

    // Images and views
    std::vector<std::unique_ptr<VulkanTexture>> m_images;
    std::vector<std::unique_ptr<VulkanTextureView>> m_views;

    // State
    u32 m_current_image_index{0};
    bool m_needs_recreation{false};
    bool m_vsync{true};

    // Stored for recreation
    void* m_window_handle{nullptr};
};

} // namespace hz::rhi::vk
