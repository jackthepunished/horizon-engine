/**
 * @file vk_swapchain.cpp
 * @brief Vulkan Swapchain implementation
 */

#include "vk_swapchain.hpp"

#include "vk_device.hpp"
#include "vk_sync.hpp"
#include "vk_texture.hpp"

// Include GLFW for surface creation
// Note: This assumes GLFW is used for windowing. If using a different windowing system,
// replace this with the appropriate surface creation mechanism.
#define GLFW_INCLUDE_NONE
#include <algorithm>

#include <GLFW/glfw3.h>

namespace hz::rhi::vk {

// ============================================================================
// Static Helpers
// ============================================================================

SwapchainSupportDetails VulkanSwapchain::query_support(VkPhysicalDevice device,
                                                       VkSurfaceKHR surface) {
    SwapchainSupportDetails details;

    // Get surface capabilities
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    // Get surface formats
    u32 format_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count, nullptr);
    if (format_count > 0) {
        details.formats.resize(format_count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count,
                                             details.formats.data());
    }

    // Get present modes
    u32 present_mode_count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_mode_count, nullptr);
    if (present_mode_count > 0) {
        details.present_modes.resize(present_mode_count);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_mode_count,
                                                  details.present_modes.data());
    }

    return details;
}

// ============================================================================
// Constructor / Destructor
// ============================================================================

VulkanSwapchain::VulkanSwapchain(VulkanDevice& device, const SwapchainDesc& desc)
    : m_device(device), m_vsync(desc.vsync), m_window_handle(desc.window_handle) {

    // Create surface
    if (!create_surface(desc.window_handle)) {
        HZ_LOG_CRITICAL("Failed to create Vulkan surface");
        return;
    }

    // Check present support for graphics queue
    VkBool32 present_support = VK_FALSE;
    vkGetPhysicalDeviceSurfaceSupportKHR(m_device.physical_device(),
                                         m_device.queue_families().graphics, m_surface,
                                         &present_support);

    if (!present_support) {
        HZ_LOG_CRITICAL("Graphics queue does not support presentation");
        return;
    }

    // Store preferred format for selection
    m_format = desc.format;

    // Create swapchain
    if (!create_swapchain(desc.width, desc.height, desc.vsync)) {
        HZ_LOG_CRITICAL("Failed to create Vulkan swapchain");
        return;
    }

    // Create image views
    create_image_views();

    if (desc.debug_name) {
        m_device.set_debug_name(reinterpret_cast<u64>(m_swapchain), desc.debug_name);
    }

    HZ_LOG_INFO("Created Vulkan swapchain: {}x{}, {} images, format {}", m_width, m_height,
                m_images.size(), static_cast<int>(m_vk_format));
}

VulkanSwapchain::~VulkanSwapchain() {
    // Wait for device to be idle before destroying
    m_device.wait_idle();

    cleanup_swapchain();

    if (m_surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(m_device.instance(), m_surface, nullptr);
        m_surface = VK_NULL_HANDLE;
    }
}

// ============================================================================
// Surface Creation
// ============================================================================

bool VulkanSwapchain::create_surface(void* window_handle) {
    // Use GLFW to create the surface
    auto* window = static_cast<GLFWwindow*>(window_handle);

    VkResult result = glfwCreateWindowSurface(m_device.instance(), window, nullptr, &m_surface);

    if (result != VK_SUCCESS) {
        HZ_LOG_ERROR("glfwCreateWindowSurface failed: {}", vk_result_string(result));
        return false;
    }

    return true;
}

// ============================================================================
// Swapchain Creation
// ============================================================================

bool VulkanSwapchain::create_swapchain(u32 width, u32 height, bool vsync) {
    SwapchainSupportDetails support = query_support(m_device.physical_device(), m_surface);

    if (!support.is_adequate()) {
        HZ_LOG_ERROR("Swapchain support is inadequate");
        return false;
    }

    // Choose surface format
    VkSurfaceFormatKHR surface_format = choose_surface_format(support.formats, m_format);
    m_vk_format = surface_format.format;
    m_color_space = surface_format.colorSpace;
    m_format = from_vk_format(m_vk_format);

    // Choose present mode
    m_present_mode = choose_present_mode(support.present_modes, vsync);

    // Choose extent
    VkExtent2D extent = choose_extent(support.capabilities, width, height);
    m_width = extent.width;
    m_height = extent.height;

    // Determine image count (prefer triple buffering)
    u32 image_count = support.capabilities.minImageCount + 1;
    if (support.capabilities.maxImageCount > 0 &&
        image_count > support.capabilities.maxImageCount) {
        image_count = support.capabilities.maxImageCount;
    }

    // Create swapchain
    VkSwapchainCreateInfoKHR create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = m_surface;
    create_info.minImageCount = image_count;
    create_info.imageFormat = m_vk_format;
    create_info.imageColorSpace = m_color_space;
    create_info.imageExtent = extent;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    // Handle queue family sharing
    u32 queue_family_indices[] = {m_device.queue_families().graphics,
                                  m_device.queue_families().present};

    if (m_device.queue_families().graphics != m_device.queue_families().present) {
        create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        create_info.queueFamilyIndexCount = 2;
        create_info.pQueueFamilyIndices = queue_family_indices;
    } else {
        create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    create_info.preTransform = support.capabilities.currentTransform;
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create_info.presentMode = m_present_mode;
    create_info.clipped = VK_TRUE;
    create_info.oldSwapchain = m_swapchain; // For recreation

    VkSwapchainKHR new_swapchain = VK_NULL_HANDLE;
    VkResult result =
        vkCreateSwapchainKHR(m_device.device(), &create_info, nullptr, &new_swapchain);

    // Clean up old swapchain if it exists
    if (m_swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(m_device.device(), m_swapchain, nullptr);
    }

    m_swapchain = new_swapchain;

    if (result != VK_SUCCESS) {
        HZ_LOG_ERROR("vkCreateSwapchainKHR failed: {}", vk_result_string(result));
        return false;
    }

    return true;
}

void VulkanSwapchain::create_image_views() {
    // Get swapchain images
    u32 image_count = 0;
    vkGetSwapchainImagesKHR(m_device.device(), m_swapchain, &image_count, nullptr);

    std::vector<VkImage> swapchain_images(image_count);
    vkGetSwapchainImagesKHR(m_device.device(), m_swapchain, &image_count, swapchain_images.data());

    // Clear old images and views
    m_images.clear();
    m_views.clear();

    m_images.reserve(image_count);
    m_views.reserve(image_count);

    // Create texture wrappers and views for each swapchain image
    TextureDesc texture_desc{};
    texture_desc.type = TextureType::Texture2D;
    texture_desc.format = m_format;
    texture_desc.width = m_width;
    texture_desc.height = m_height;
    texture_desc.depth = 1;
    texture_desc.mip_levels = 1;
    texture_desc.array_layers = 1;
    texture_desc.sample_count = 1;
    texture_desc.usage = TextureUsage::RenderTarget;

    for (u32 i = 0; i < image_count; ++i) {
        // Create texture wrapper (does not own the image)
        auto texture = std::make_unique<VulkanTexture>(m_device, swapchain_images[i], texture_desc,
                                                       false /* owns_image */);

        // Create view
        TextureViewDesc view_desc{};
        view_desc.texture = texture.get();
        view_desc.view_type = TextureType::Texture2D;
        view_desc.format = m_format;
        view_desc.base_mip_level = 0;
        view_desc.mip_level_count = 1;
        view_desc.base_array_layer = 0;
        view_desc.array_layer_count = 1;

        auto view = std::make_unique<VulkanTextureView>(m_device, view_desc);

        m_images.push_back(std::move(texture));
        m_views.push_back(std::move(view));
    }
}

void VulkanSwapchain::cleanup_swapchain() {
    // Clear images and views (views must be destroyed before images)
    m_views.clear();
    m_images.clear();

    if (m_swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(m_device.device(), m_swapchain, nullptr);
        m_swapchain = VK_NULL_HANDLE;
    }
}

// ============================================================================
// Selection Helpers
// ============================================================================

VkSurfaceFormatKHR
VulkanSwapchain::choose_surface_format(const std::vector<VkSurfaceFormatKHR>& available_formats,
                                       Format preferred) const {
    VkFormat preferred_vk = to_vk_format(preferred);

    // Try to find exact match
    for (const auto& format : available_formats) {
        if (format.format == preferred_vk &&
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }

    // Try common formats
    const VkFormat fallback_formats[] = {
        VK_FORMAT_B8G8R8A8_SRGB,
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_FORMAT_B8G8R8A8_UNORM,
        VK_FORMAT_R8G8B8A8_UNORM,
    };

    for (VkFormat fallback : fallback_formats) {
        for (const auto& format : available_formats) {
            if (format.format == fallback &&
                format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return format;
            }
        }
    }

    // Return first available format
    return available_formats[0];
}

VkPresentModeKHR
VulkanSwapchain::choose_present_mode(const std::vector<VkPresentModeKHR>& available_modes,
                                     bool vsync) const {
    if (!vsync) {
        // Prefer mailbox (triple buffering without tearing)
        for (const auto& mode : available_modes) {
            if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
                return mode;
            }
        }

        // Fall back to immediate (no vsync, may tear)
        for (const auto& mode : available_modes) {
            if (mode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
                return mode;
            }
        }
    }

    // Default to FIFO (vsync, always available)
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanSwapchain::choose_extent(const VkSurfaceCapabilitiesKHR& capabilities, u32 width,
                                          u32 height) const {
    // Special value indicates we should use the window size
    if (capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent;
    }

    // Clamp to surface capabilities
    VkExtent2D extent = {width, height};
    extent.width = std::clamp(extent.width, capabilities.minImageExtent.width,
                              capabilities.maxImageExtent.width);
    extent.height = std::clamp(extent.height, capabilities.minImageExtent.height,
                               capabilities.maxImageExtent.height);

    return extent;
}

// ============================================================================
// Texture/View Access
// ============================================================================

Texture* VulkanSwapchain::get_current_texture() {
    if (m_current_image_index >= m_images.size()) {
        return nullptr;
    }
    return m_images[m_current_image_index].get();
}

TextureView* VulkanSwapchain::get_current_view() {
    if (m_current_image_index >= m_views.size()) {
        return nullptr;
    }
    return m_views[m_current_image_index].get();
}

VulkanTexture* VulkanSwapchain::get_texture(u32 index) const {
    if (index >= m_images.size()) {
        return nullptr;
    }
    return m_images[index].get();
}

VulkanTextureView* VulkanSwapchain::get_view(u32 index) const {
    if (index >= m_views.size()) {
        return nullptr;
    }
    return m_views[index].get();
}

// ============================================================================
// Frame Operations
// ============================================================================

bool VulkanSwapchain::acquire_next_image(Semaphore* signal_semaphore) {
    VkSemaphore vk_semaphore = VK_NULL_HANDLE;
    if (signal_semaphore) {
        auto* vk_sem = static_cast<VulkanSemaphore*>(signal_semaphore);
        vk_semaphore = vk_sem->semaphore();
    }

    VkResult result = vkAcquireNextImageKHR(m_device.device(), m_swapchain, UINT64_MAX,
                                            vk_semaphore, VK_NULL_HANDLE, &m_current_image_index);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        m_needs_recreation = true;
        return false;
    }

    if (result == VK_SUBOPTIMAL_KHR) {
        m_needs_recreation = true;
        // Continue with presentation, but mark for recreation
    }

    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        HZ_LOG_ERROR("vkAcquireNextImageKHR failed: {}", vk_result_string(result));
        return false;
    }

    return true;
}

void VulkanSwapchain::present(std::span<Semaphore* const> wait_semaphores) {
    std::vector<VkSemaphore> vk_semaphores;
    vk_semaphores.reserve(wait_semaphores.size());

    for (auto* sem : wait_semaphores) {
        auto* vk_sem = static_cast<VulkanSemaphore*>(sem);
        vk_semaphores.push_back(vk_sem->semaphore());
    }

    VkPresentInfoKHR present_info{};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = static_cast<u32>(vk_semaphores.size());
    present_info.pWaitSemaphores = vk_semaphores.data();
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &m_swapchain;
    present_info.pImageIndices = &m_current_image_index;

    VkResult result = vkQueuePresentKHR(m_device.graphics_queue(), &present_info);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        m_needs_recreation = true;
    } else if (result != VK_SUCCESS) {
        HZ_LOG_ERROR("vkQueuePresentKHR failed: {}", vk_result_string(result));
    }
}

void VulkanSwapchain::resize(u32 width, u32 height) {
    // Wait for device to be idle
    m_device.wait_idle();

    // Cleanup old swapchain resources (but not the swapchain itself - it's used as oldSwapchain)
    m_views.clear();
    m_images.clear();

    // Recreate swapchain
    if (!create_swapchain(width, height, m_vsync)) {
        HZ_LOG_ERROR("Failed to recreate swapchain");
        return;
    }

    // Recreate image views
    create_image_views();

    m_needs_recreation = false;

    HZ_LOG_INFO("Resized swapchain to {}x{}", m_width, m_height);
}

} // namespace hz::rhi::vk
