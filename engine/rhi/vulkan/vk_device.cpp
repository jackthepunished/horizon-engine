#include "vk_device.hpp"

#include "vk_buffer.hpp"
#include "vk_command_list.hpp"
#include "vk_descriptor.hpp"
#include "vk_pipeline.hpp"
#include "vk_sampler.hpp"
#include "vk_swapchain.hpp"
#include "vk_sync.hpp"
#include "vk_texture.hpp"

#include <algorithm>
#include <cstring>
#include <set>

#include <GLFW/glfw3.h>

// VMA implementation
#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#include <vk_mem_alloc.h>

namespace hz::rhi::vk {

// ============================================================================
// Debug Callback
// ============================================================================

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT type,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data, void* user_data) {
    auto* device = static_cast<VulkanDevice*>(user_data);

    const bool is_error = severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

    if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        if (is_error) {
            HZ_LOG_ERROR("Vulkan Validation: {}", callback_data->pMessage);
        } else {
            HZ_LOG_WARN("Vulkan Validation: {}", callback_data->pMessage);
        }
    }

    HZ_UNUSED(type);
    HZ_UNUSED(device);

    return VK_FALSE;
}

// ============================================================================
// Constructor / Destructor
// ============================================================================

VulkanDevice::VulkanDevice(const DeviceDesc& desc)
    : m_validation_enabled(desc.enable_validation), m_debug_callback(desc.debug_callback) {

    // Initialize volk
    VK_CHECK_FATAL(volkInitialize());

    // Create instance
    if (!create_instance(desc)) {
        HZ_LOG_CRITICAL("Failed to create Vulkan instance");
        return;
    }

    // Load instance functions
    volkLoadInstance(m_instance);

    // Setup debug messenger
    if (m_validation_enabled) {
        setup_debug_messenger();
    }

    // Select physical device
    if (!select_physical_device(desc.prefer_discrete_gpu)) {
        HZ_LOG_CRITICAL("Failed to select physical device");
        return;
    }

    // Create logical device
    if (!create_logical_device()) {
        HZ_LOG_CRITICAL("Failed to create logical device");
        return;
    }

    // Load device functions
    volkLoadDevice(m_device);

    // Create VMA allocator
    if (!create_allocator()) {
        HZ_LOG_CRITICAL("Failed to create VMA allocator");
        return;
    }

    // Create per-frame resources
    if (!create_frame_resources()) {
        HZ_LOG_CRITICAL("Failed to create frame resources");
        return;
    }

    // Populate device info
    populate_device_info();
    populate_device_limits();

    HZ_LOG_INFO("Vulkan device initialized: {}", m_device_info.name);
}

VulkanDevice::~VulkanDevice() {
    if (m_device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(m_device);
    }

    // Clean up frame resources
    destroy_frame_resources();

    // Destroy VMA allocator
    if (m_allocator != VK_NULL_HANDLE) {
        vmaDestroyAllocator(m_allocator);
    }

    // Destroy logical device
    if (m_device != VK_NULL_HANDLE) {
        vkDestroyDevice(m_device, nullptr);
    }

    // Destroy debug messenger
    if (m_debug_messenger != VK_NULL_HANDLE) {
        vkDestroyDebugUtilsMessengerEXT(m_instance, m_debug_messenger, nullptr);
    }

    // Destroy instance
    if (m_instance != VK_NULL_HANDLE) {
        vkDestroyInstance(m_instance, nullptr);
    }
}

// ============================================================================
// Instance Creation
// ============================================================================

bool VulkanDevice::create_instance(const DeviceDesc& desc) {
    // Check for validation layer support
    std::vector<const char*> validation_layers;
    if (m_validation_enabled) {
        u32 layer_count = 0;
        vkEnumerateInstanceLayerProperties(&layer_count, nullptr);

        std::vector<VkLayerProperties> available_layers(layer_count);
        vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data());

        const char* desired_layer = "VK_LAYER_KHRONOS_validation";
        bool found = false;
        for (const auto& layer : available_layers) {
            if (std::strcmp(layer.layerName, desired_layer) == 0) {
                found = true;
                break;
            }
        }

        if (found) {
            validation_layers.push_back(desired_layer);
        } else {
            HZ_LOG_WARN("Validation layer not found, disabling validation");
            m_validation_enabled = false;
        }
    }

    // Get required extensions from GLFW
    u32 glfw_extension_count = 0;
    const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);

    std::vector<const char*> extensions(glfw_extensions, glfw_extensions + glfw_extension_count);

    if (m_validation_enabled) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    // Portability enumeration for macOS
#ifdef __APPLE__
    extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
    extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
#endif

    // Application info
    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = desc.application_name;
    app_info.applicationVersion = desc.application_version;
    app_info.pEngineName = "Horizon Engine";
    app_info.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    app_info.apiVersion = VK_API_VERSION_1_3;

    // Instance create info
    VkInstanceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;
    create_info.enabledExtensionCount = static_cast<u32>(extensions.size());
    create_info.ppEnabledExtensionNames = extensions.data();
    create_info.enabledLayerCount = static_cast<u32>(validation_layers.size());
    create_info.ppEnabledLayerNames = validation_layers.data();

#ifdef __APPLE__
    create_info.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

    // Debug messenger for instance creation/destruction
    VkDebugUtilsMessengerCreateInfoEXT debug_create_info{};
    if (m_validation_enabled) {
        debug_create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debug_create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debug_create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debug_create_info.pfnUserCallback = debug_callback;
        debug_create_info.pUserData = this;

        create_info.pNext = &debug_create_info;
    }

    return VK_CHECK(vkCreateInstance(&create_info, nullptr, &m_instance));
}

bool VulkanDevice::setup_debug_messenger() {
    VkDebugUtilsMessengerCreateInfoEXT create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                  VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                  VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                              VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                              VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    create_info.pfnUserCallback = debug_callback;
    create_info.pUserData = this;

    return VK_CHECK(
        vkCreateDebugUtilsMessengerEXT(m_instance, &create_info, nullptr, &m_debug_messenger));
}

// ============================================================================
// Physical Device Selection
// ============================================================================

bool VulkanDevice::select_physical_device(bool prefer_discrete) {
    u32 device_count = 0;
    vkEnumeratePhysicalDevices(m_instance, &device_count, nullptr);

    if (device_count == 0) {
        HZ_LOG_ERROR("No Vulkan-capable GPUs found");
        return false;
    }

    std::vector<VkPhysicalDevice> devices(device_count);
    vkEnumeratePhysicalDevices(m_instance, &device_count, devices.data());

    // Score and select best device
    int best_score = -1;
    for (const auto& device : devices) {
        int score = rate_device_suitability(device);
        if (score > best_score) {
            best_score = score;
            m_physical_device = device;
        }
    }

    if (m_physical_device == VK_NULL_HANDLE) {
        HZ_LOG_ERROR("Failed to find suitable GPU");
        return false;
    }

    m_queue_families = find_queue_families(m_physical_device);

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(m_physical_device, &props);
    HZ_LOG_INFO("Selected GPU: {}", props.deviceName);

    return true;
}

int VulkanDevice::rate_device_suitability(VkPhysicalDevice device) const {
    VkPhysicalDeviceProperties props;
    VkPhysicalDeviceFeatures features;
    vkGetPhysicalDeviceProperties(device, &props);
    vkGetPhysicalDeviceFeatures(device, &features);

    // Check required features
    if (!features.samplerAnisotropy) {
        return -1;
    }

    // Check queue families
    QueueFamilyIndices indices = find_queue_families(device);
    if (!indices.is_complete()) {
        return -1;
    }

    // Check extension support
    if (!check_device_extension_support(device)) {
        return -1;
    }

    int score = 0;

    // Discrete GPUs have significant performance advantage
    if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        score += 10000;
    } else if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
        score += 1000;
    }

    // Higher VRAM is better
    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(device, &mem_props);

    VkDeviceSize total_vram = 0;
    for (u32 i = 0; i < mem_props.memoryHeapCount; ++i) {
        if (mem_props.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            total_vram += mem_props.memoryHeaps[i].size;
        }
    }
    score += static_cast<int>(total_vram / (1024 * 1024)); // MB of VRAM

    // Larger max texture size is better
    score += static_cast<int>(props.limits.maxImageDimension2D / 1024);

    return score;
}

QueueFamilyIndices VulkanDevice::find_queue_families(VkPhysicalDevice device) const {
    QueueFamilyIndices indices;

    u32 queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);

    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families.data());

    for (u32 i = 0; i < queue_families.size(); ++i) {
        const auto& family = queue_families[i];

        // Graphics queue (also supports compute and transfer)
        if (family.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphics = i;
            indices.present = i; // Assume graphics queue supports present
        }

        // Dedicated compute queue
        if ((family.queueFlags & VK_QUEUE_COMPUTE_BIT) &&
            !(family.queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
            indices.compute = i;
        }

        // Dedicated transfer queue
        if ((family.queueFlags & VK_QUEUE_TRANSFER_BIT) &&
            !(family.queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
            !(family.queueFlags & VK_QUEUE_COMPUTE_BIT)) {
            indices.transfer = i;
        }
    }

    // Fallback: use graphics queue for compute/transfer if no dedicated queues
    if (indices.compute == UINT32_MAX) {
        indices.compute = indices.graphics;
    }
    if (indices.transfer == UINT32_MAX) {
        indices.transfer = indices.graphics;
    }

    return indices;
}

bool VulkanDevice::check_device_extension_support(VkPhysicalDevice device) const {
    u32 extension_count = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, nullptr);

    std::vector<VkExtensionProperties> available_extensions(extension_count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count,
                                         available_extensions.data());

    std::set<std::string> required_extensions(REQUIRED_DEVICE_EXTENSIONS.begin(),
                                              REQUIRED_DEVICE_EXTENSIONS.end());

    for (const auto& ext : available_extensions) {
        required_extensions.erase(ext.extensionName);
    }

    return required_extensions.empty();
}

// ============================================================================
// Logical Device Creation
// ============================================================================

bool VulkanDevice::create_logical_device() {
    // Collect unique queue families
    std::set<u32> unique_families = {m_queue_families.graphics};
    if (m_queue_families.compute != m_queue_families.graphics) {
        unique_families.insert(m_queue_families.compute);
    }
    if (m_queue_families.transfer != m_queue_families.graphics &&
        m_queue_families.transfer != m_queue_families.compute) {
        unique_families.insert(m_queue_families.transfer);
    }

    // Create queue create infos
    std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
    float queue_priority = 1.0f;

    for (u32 family : unique_families) {
        VkDeviceQueueCreateInfo queue_create_info{};
        queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_create_info.queueFamilyIndex = family;
        queue_create_info.queueCount = 1;
        queue_create_info.pQueuePriorities = &queue_priority;
        queue_create_infos.push_back(queue_create_info);
    }

    // Required features
    VkPhysicalDeviceFeatures device_features{};
    device_features.samplerAnisotropy = VK_TRUE;
    device_features.fillModeNonSolid = VK_TRUE;  // Wireframe
    device_features.wideLines = VK_TRUE;         // Line width
    device_features.multiDrawIndirect = VK_TRUE; // MDI
    device_features.drawIndirectFirstInstance = VK_TRUE;

    // Vulkan 1.2 features
    VkPhysicalDeviceVulkan12Features features12{};
    features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    features12.descriptorIndexing = VK_TRUE;
    features12.bufferDeviceAddress = VK_TRUE;
    features12.timelineSemaphore = VK_TRUE;
    features12.drawIndirectCount = VK_TRUE;

    // Vulkan 1.3 features (dynamic rendering)
    VkPhysicalDeviceVulkan13Features features13{};
    features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    features13.pNext = &features12;
    features13.dynamicRendering = VK_TRUE;
    features13.synchronization2 = VK_TRUE;

    // Extensions
    std::vector<const char*> extensions(REQUIRED_DEVICE_EXTENSIONS.begin(),
                                        REQUIRED_DEVICE_EXTENSIONS.end());

#ifdef __APPLE__
    extensions.push_back("VK_KHR_portability_subset");
#endif

    // Create device
    VkDeviceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.pNext = &features13;
    create_info.queueCreateInfoCount = static_cast<u32>(queue_create_infos.size());
    create_info.pQueueCreateInfos = queue_create_infos.data();
    create_info.pEnabledFeatures = &device_features;
    create_info.enabledExtensionCount = static_cast<u32>(extensions.size());
    create_info.ppEnabledExtensionNames = extensions.data();

    if (!VK_CHECK(vkCreateDevice(m_physical_device, &create_info, nullptr, &m_device))) {
        return false;
    }

    // Get queues
    vkGetDeviceQueue(m_device, m_queue_families.graphics, 0, &m_graphics_queue);
    vkGetDeviceQueue(m_device, m_queue_families.compute, 0, &m_compute_queue);
    vkGetDeviceQueue(m_device, m_queue_families.transfer, 0, &m_transfer_queue);

    return true;
}

// ============================================================================
// VMA Allocator
// ============================================================================

bool VulkanDevice::create_allocator() {
    VmaVulkanFunctions vulkan_functions{};
    vulkan_functions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    vulkan_functions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

    VmaAllocatorCreateInfo allocator_info{};
    allocator_info.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    allocator_info.physicalDevice = m_physical_device;
    allocator_info.device = m_device;
    allocator_info.instance = m_instance;
    allocator_info.vulkanApiVersion = VK_API_VERSION_1_3;
    allocator_info.pVulkanFunctions = &vulkan_functions;

    return VK_CHECK(vmaCreateAllocator(&allocator_info, &m_allocator));
}

// ============================================================================
// Frame Resources
// ============================================================================

bool VulkanDevice::create_frame_resources() {
    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        auto& frame = m_frames[i];

        // Command pool
        VkCommandPoolCreateInfo pool_info{};
        pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        pool_info.queueFamilyIndex = m_queue_families.graphics;

        if (!VK_CHECK(vkCreateCommandPool(m_device, &pool_info, nullptr, &frame.command_pool))) {
            return false;
        }

        // In-flight fence (signaled initially so first wait doesn't block)
        VkFenceCreateInfo fence_info{};
        fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        if (!VK_CHECK(vkCreateFence(m_device, &fence_info, nullptr, &frame.in_flight_fence))) {
            return false;
        }

        // Semaphores
        VkSemaphoreCreateInfo semaphore_info{};
        semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        if (!VK_CHECK(vkCreateSemaphore(m_device, &semaphore_info, nullptr,
                                        &frame.image_available_semaphore))) {
            return false;
        }

        if (!VK_CHECK(vkCreateSemaphore(m_device, &semaphore_info, nullptr,
                                        &frame.render_finished_semaphore))) {
            return false;
        }
    }

    return true;
}

void VulkanDevice::destroy_frame_resources() {
    for (auto& frame : m_frames) {
        // Process any remaining deletions
        process_deletion_queue(frame);

        if (frame.command_pool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(m_device, frame.command_pool, nullptr);
        }
        if (frame.in_flight_fence != VK_NULL_HANDLE) {
            vkDestroyFence(m_device, frame.in_flight_fence, nullptr);
        }
        if (frame.image_available_semaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(m_device, frame.image_available_semaphore, nullptr);
        }
        if (frame.render_finished_semaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(m_device, frame.render_finished_semaphore, nullptr);
        }
    }
}

// ============================================================================
// Device Info
// ============================================================================

void VulkanDevice::populate_device_info() {
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(m_physical_device, &props);

    std::strncpy(m_device_info.name, props.deviceName, sizeof(m_device_info.name) - 1);
    m_device_info.vendor_id = props.vendorID;
    m_device_info.device_id = props.deviceID;
    m_device_info.driver_version = props.driverVersion;

    // Device type
    switch (props.deviceType) {
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
        m_device_info.type = DeviceType::DiscreteGPU;
        break;
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
        m_device_info.type = DeviceType::IntegratedGPU;
        break;
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
        m_device_info.type = DeviceType::VirtualGPU;
        break;
    case VK_PHYSICAL_DEVICE_TYPE_CPU:
        m_device_info.type = DeviceType::CPU;
        break;
    default:
        m_device_info.type = DeviceType::Other;
        break;
    }

    // Vendor
    switch (props.vendorID) {
    case 0x1002:
        m_device_info.vendor = Vendor::AMD;
        break;
    case 0x10DE:
        m_device_info.vendor = Vendor::NVIDIA;
        break;
    case 0x8086:
        m_device_info.vendor = Vendor::Intel;
        break;
    case 0x13B5:
        m_device_info.vendor = Vendor::ARM;
        break;
    case 0x5143:
        m_device_info.vendor = Vendor::Qualcomm;
        break;
    case 0x106B:
        m_device_info.vendor = Vendor::Apple;
        break;
    default:
        m_device_info.vendor = Vendor::Unknown;
        break;
    }

    // Memory info
    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(m_physical_device, &mem_props);

    for (u32 i = 0; i < mem_props.memoryHeapCount; ++i) {
        if (mem_props.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            m_device_info.dedicated_video_memory += mem_props.memoryHeaps[i].size;
        } else {
            m_device_info.shared_system_memory += mem_props.memoryHeaps[i].size;
        }
    }
}

void VulkanDevice::populate_device_limits() {
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(m_physical_device, &props);

    const auto& limits = props.limits;

    m_limits.max_texture_dimension_1d = limits.maxImageDimension1D;
    m_limits.max_texture_dimension_2d = limits.maxImageDimension2D;
    m_limits.max_texture_dimension_3d = limits.maxImageDimension3D;
    m_limits.max_texture_dimension_cube = limits.maxImageDimensionCube;
    m_limits.max_texture_array_layers = limits.maxImageArrayLayers;
    m_limits.max_uniform_buffer_size = limits.maxUniformBufferRange;
    m_limits.max_storage_buffer_size = limits.maxStorageBufferRange;
    m_limits.max_push_constant_size = limits.maxPushConstantsSize;
    m_limits.max_bound_descriptor_sets = limits.maxBoundDescriptorSets;
    m_limits.max_vertex_input_attributes = limits.maxVertexInputAttributes;
    m_limits.max_vertex_input_bindings = limits.maxVertexInputBindings;
    m_limits.max_vertex_input_attribute_offset = limits.maxVertexInputAttributeOffset;
    m_limits.max_vertex_input_binding_stride = limits.maxVertexInputBindingStride;
    m_limits.max_color_attachments = limits.maxColorAttachments;
    m_limits.max_compute_work_group_count[0] = limits.maxComputeWorkGroupCount[0];
    m_limits.max_compute_work_group_count[1] = limits.maxComputeWorkGroupCount[1];
    m_limits.max_compute_work_group_count[2] = limits.maxComputeWorkGroupCount[2];
    m_limits.max_compute_work_group_size[0] = limits.maxComputeWorkGroupSize[0];
    m_limits.max_compute_work_group_size[1] = limits.maxComputeWorkGroupSize[1];
    m_limits.max_compute_work_group_size[2] = limits.maxComputeWorkGroupSize[2];
    m_limits.max_compute_work_group_invocations = limits.maxComputeWorkGroupInvocations;
    m_limits.max_sampler_anisotropy = limits.maxSamplerAnisotropy;
    m_limits.min_uniform_buffer_offset_alignment =
        static_cast<u32>(limits.minUniformBufferOffsetAlignment);
    m_limits.min_storage_buffer_offset_alignment =
        static_cast<u32>(limits.minStorageBufferOffsetAlignment);
    m_limits.timestamp_period_ns = static_cast<u32>(limits.timestampPeriod);

    // Feature support
    VkPhysicalDeviceFeatures features;
    vkGetPhysicalDeviceFeatures(m_physical_device, &features);

    m_limits.supports_geometry_shader = features.geometryShader == VK_TRUE;
    m_limits.supports_tessellation = features.tessellationShader == VK_TRUE;
    m_limits.supports_compute = true; // Always available in Vulkan 1.0+
    m_limits.supports_multi_draw_indirect = features.multiDrawIndirect == VK_TRUE;
}

// ============================================================================
// Frame Management
// ============================================================================

u32 VulkanDevice::begin_frame() {
    auto& frame = m_frames[m_current_frame];

    // Wait for previous frame to finish
    vkWaitForFences(m_device, 1, &frame.in_flight_fence, VK_TRUE, UINT64_MAX);

    // Process deletion queue for this frame (resources are now safe to delete)
    process_deletion_queue(frame);

    return m_current_frame;
}

void VulkanDevice::end_frame() {
    auto& frame = m_frames[m_current_frame];

    // Reset fence for next use
    vkResetFences(m_device, 1, &frame.in_flight_fence);

    // Advance frame
    m_current_frame = (m_current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
    ++m_frame_number;
}

// ============================================================================
// Deferred Deletion
// ============================================================================

void VulkanDevice::defer_deletion(VkBuffer buffer, VmaAllocation allocation) {
    auto& frame = m_frames[m_current_frame];
    frame.buffers_to_delete.push_back(buffer);
    frame.allocations_to_delete.push_back(allocation);
}

void VulkanDevice::defer_deletion(VkImage image, VmaAllocation allocation) {
    auto& frame = m_frames[m_current_frame];
    frame.images_to_delete.push_back(image);
    frame.allocations_to_delete.push_back(allocation);
}

void VulkanDevice::defer_deletion(VkImageView view) {
    m_frames[m_current_frame].image_views_to_delete.push_back(view);
}

void VulkanDevice::defer_deletion(VkSampler sampler) {
    m_frames[m_current_frame].samplers_to_delete.push_back(sampler);
}

void VulkanDevice::defer_deletion(VkPipeline pipeline) {
    m_frames[m_current_frame].pipelines_to_delete.push_back(pipeline);
}

void VulkanDevice::defer_deletion(VkPipelineLayout layout) {
    m_frames[m_current_frame].pipeline_layouts_to_delete.push_back(layout);
}

void VulkanDevice::defer_deletion(VkRenderPass render_pass) {
    m_frames[m_current_frame].render_passes_to_delete.push_back(render_pass);
}

void VulkanDevice::defer_deletion(VkFramebuffer framebuffer) {
    m_frames[m_current_frame].framebuffers_to_delete.push_back(framebuffer);
}

void VulkanDevice::defer_deletion(VkDescriptorSetLayout layout) {
    m_frames[m_current_frame].descriptor_set_layouts_to_delete.push_back(layout);
}

void VulkanDevice::defer_deletion(VkDescriptorPool pool) {
    m_frames[m_current_frame].descriptor_pools_to_delete.push_back(pool);
}

void VulkanDevice::defer_deletion(VkShaderModule module) {
    m_frames[m_current_frame].shader_modules_to_delete.push_back(module);
}

void VulkanDevice::process_deletion_queue(FrameData& frame) {
    // Destroy buffers
    for (size_t i = 0; i < frame.buffers_to_delete.size(); ++i) {
        vmaDestroyBuffer(m_allocator, frame.buffers_to_delete[i], frame.allocations_to_delete[i]);
    }
    frame.buffers_to_delete.clear();

    // Destroy images (allocations are shared with buffers for now)
    for (auto image : frame.images_to_delete) {
        vkDestroyImage(m_device, image, nullptr);
    }
    frame.images_to_delete.clear();
    frame.allocations_to_delete.clear();

    // Destroy image views
    for (auto view : frame.image_views_to_delete) {
        vkDestroyImageView(m_device, view, nullptr);
    }
    frame.image_views_to_delete.clear();

    // Destroy samplers
    for (auto sampler : frame.samplers_to_delete) {
        vkDestroySampler(m_device, sampler, nullptr);
    }
    frame.samplers_to_delete.clear();

    // Destroy pipelines
    for (auto pipeline : frame.pipelines_to_delete) {
        vkDestroyPipeline(m_device, pipeline, nullptr);
    }
    frame.pipelines_to_delete.clear();

    // Destroy pipeline layouts
    for (auto layout : frame.pipeline_layouts_to_delete) {
        vkDestroyPipelineLayout(m_device, layout, nullptr);
    }
    frame.pipeline_layouts_to_delete.clear();

    // Destroy render passes
    for (auto render_pass : frame.render_passes_to_delete) {
        vkDestroyRenderPass(m_device, render_pass, nullptr);
    }
    frame.render_passes_to_delete.clear();

    // Destroy framebuffers
    for (auto framebuffer : frame.framebuffers_to_delete) {
        vkDestroyFramebuffer(m_device, framebuffer, nullptr);
    }
    frame.framebuffers_to_delete.clear();

    // Destroy descriptor set layouts
    for (auto layout : frame.descriptor_set_layouts_to_delete) {
        vkDestroyDescriptorSetLayout(m_device, layout, nullptr);
    }
    frame.descriptor_set_layouts_to_delete.clear();

    // Destroy descriptor pools
    for (auto pool : frame.descriptor_pools_to_delete) {
        vkDestroyDescriptorPool(m_device, pool, nullptr);
    }
    frame.descriptor_pools_to_delete.clear();

    // Destroy shader modules
    for (auto module : frame.shader_modules_to_delete) {
        vkDestroyShaderModule(m_device, module, nullptr);
    }
    frame.shader_modules_to_delete.clear();
}

// ============================================================================
// Queue Operations
// ============================================================================

void VulkanDevice::submit(QueueType queue_type, std::span<const SubmitInfo> submits) {
    VkQueue queue = get_queue(queue_type);

    for (const auto& submit : submits) {
        std::vector<VkCommandBuffer> cmd_buffers;
        cmd_buffers.reserve(submit.command_lists.size());
        for (auto* cmd : submit.command_lists) {
            cmd_buffers.push_back(reinterpret_cast<VkCommandBuffer>(cmd->native_handle()));
        }

        std::vector<VkSemaphore> wait_semaphores;
        wait_semaphores.reserve(submit.wait_semaphores.size());
        for (auto* sem : submit.wait_semaphores) {
            wait_semaphores.push_back(reinterpret_cast<VkSemaphore>(sem->native_handle()));
        }

        std::vector<VkSemaphore> signal_semaphores;
        signal_semaphores.reserve(submit.signal_semaphores.size());
        for (auto* sem : submit.signal_semaphores) {
            signal_semaphores.push_back(reinterpret_cast<VkSemaphore>(sem->native_handle()));
        }

        std::vector<VkPipelineStageFlags> wait_stages(wait_semaphores.size(),
                                                      VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

        VkSubmitInfo vk_submit{};
        vk_submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        vk_submit.waitSemaphoreCount = static_cast<u32>(wait_semaphores.size());
        vk_submit.pWaitSemaphores = wait_semaphores.data();
        vk_submit.pWaitDstStageMask = wait_stages.data();
        vk_submit.commandBufferCount = static_cast<u32>(cmd_buffers.size());
        vk_submit.pCommandBuffers = cmd_buffers.data();
        vk_submit.signalSemaphoreCount = static_cast<u32>(signal_semaphores.size());
        vk_submit.pSignalSemaphores = signal_semaphores.data();

        VkFence fence = submit.signal_fence
                            ? reinterpret_cast<VkFence>(submit.signal_fence->native_handle())
                            : VK_NULL_HANDLE;

        VK_CHECK(vkQueueSubmit(queue, 1, &vk_submit, fence));
    }
}

void VulkanDevice::wait_queue_idle(QueueType queue_type) {
    vkQueueWaitIdle(get_queue(queue_type));
}

void VulkanDevice::wait_idle() {
    vkDeviceWaitIdle(m_device);
}

bool VulkanDevice::wait_fences(std::span<Fence* const> fences, bool wait_all, u64 timeout_ns) {
    std::vector<VkFence> vk_fences;
    vk_fences.reserve(fences.size());
    for (auto* fence : fences) {
        vk_fences.push_back(reinterpret_cast<VkFence>(fence->native_handle()));
    }

    VkResult result = vkWaitForFences(m_device, static_cast<u32>(vk_fences.size()),
                                      vk_fences.data(), wait_all ? VK_TRUE : VK_FALSE, timeout_ns);

    return result == VK_SUCCESS;
}

void VulkanDevice::reset_fences(std::span<Fence* const> fences) {
    std::vector<VkFence> vk_fences;
    vk_fences.reserve(fences.size());
    for (auto* fence : fences) {
        vk_fences.push_back(reinterpret_cast<VkFence>(fence->native_handle()));
    }

    vkResetFences(m_device, static_cast<u32>(vk_fences.size()), vk_fences.data());
}

// ============================================================================
// Debug
// ============================================================================

void VulkanDevice::set_debug_name(u64 handle, const char* name) {
    if (!m_validation_enabled || handle == 0 || name == nullptr) {
        return;
    }

    VkDebugUtilsObjectNameInfoEXT name_info{};
    name_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    name_info.objectType = VK_OBJECT_TYPE_UNKNOWN; // Would need to know the type
    name_info.objectHandle = handle;
    name_info.pObjectName = name;

    vkSetDebugUtilsObjectNameEXT(m_device, &name_info);
}

// ============================================================================
// Resource Creation Stubs (to be implemented in separate files)
// ============================================================================

std::unique_ptr<Buffer> VulkanDevice::create_buffer(const BufferDesc& desc) {
    return std::make_unique<VulkanBuffer>(*this, desc);
}

std::unique_ptr<Texture> VulkanDevice::create_texture(const TextureDesc& desc) {
    return std::make_unique<VulkanTexture>(*this, desc);
}

std::unique_ptr<TextureView> VulkanDevice::create_texture_view(const TextureViewDesc& desc) {
    return std::make_unique<VulkanTextureView>(*this, desc);
}

std::unique_ptr<Sampler> VulkanDevice::create_sampler(const SamplerDesc& desc) {
    return std::make_unique<VulkanSampler>(*this, desc);
}

std::unique_ptr<ShaderModule> VulkanDevice::create_shader_module(const ShaderModuleDesc& desc) {
    return std::make_unique<VulkanShaderModule>(*this, desc);
}

std::unique_ptr<RenderPass> VulkanDevice::create_render_pass(const RenderPassDesc& desc) {
    return std::make_unique<VulkanRenderPass>(*this, desc);
}

std::unique_ptr<Framebuffer> VulkanDevice::create_framebuffer(const FramebufferDesc& desc) {
    return std::make_unique<VulkanFramebuffer>(*this, desc);
}

std::unique_ptr<PipelineLayout>
VulkanDevice::create_pipeline_layout(const PipelineLayoutDesc& desc) {
    return std::make_unique<VulkanPipelineLayout>(*this, desc);
}

std::unique_ptr<Pipeline> VulkanDevice::create_graphics_pipeline(const GraphicsPipelineDesc& desc) {
    return std::make_unique<VulkanPipeline>(*this, desc);
}

std::unique_ptr<Pipeline> VulkanDevice::create_compute_pipeline(const ComputePipelineDesc& desc) {
    return std::make_unique<VulkanPipeline>(*this, desc);
}

std::unique_ptr<DescriptorSetLayout>
VulkanDevice::create_descriptor_set_layout(const DescriptorSetLayoutDesc& desc) {
    return std::make_unique<VulkanDescriptorSetLayout>(*this, desc);
}

std::unique_ptr<DescriptorPool>
VulkanDevice::create_descriptor_pool(const DescriptorPoolDesc& desc) {
    return std::make_unique<VulkanDescriptorPool>(*this, desc);
}

std::unique_ptr<Fence> VulkanDevice::create_fence(bool signaled) {
    return std::make_unique<VulkanFence>(*this, signaled);
}

std::unique_ptr<Semaphore> VulkanDevice::create_semaphore() {
    return std::make_unique<VulkanSemaphore>(*this);
}

std::unique_ptr<Swapchain> VulkanDevice::create_swapchain(const SwapchainDesc& desc) {
    return std::make_unique<VulkanSwapchain>(*this, desc);
}

std::unique_ptr<CommandList> VulkanDevice::create_command_list(QueueType queue_type) {
    return std::make_unique<VulkanCommandList>(*this, queue_type);
}

void VulkanDevice::update_buffer(Buffer& buffer, const void* data, u64 size, u64 offset) {
    auto& vk_buffer = static_cast<VulkanBuffer&>(buffer);

    if (buffer.memory_usage() == MemoryUsage::GPU_Only) {
        // Need staging buffer
        auto staging = create_staging_buffer(size, "StagingBuffer");
        staging->upload(data, size, 0);

        // Create temporary command list for copy
        ImmediateContext ctx(*this);
        ctx.submit([&](CommandList& cmd) {
            BufferCopyRegion region{0, offset, size};
            cmd.copy_buffer(*staging, buffer, {&region, 1});
        });
    } else {
        buffer.upload(data, size, offset);
    }
}

void VulkanDevice::update_texture(Texture& texture, const void* data, u64 size, u32 mip_level,
                                  u32 array_layer, Offset3D offset) {
    // Create staging buffer
    auto staging = create_staging_buffer(size, "TextureStagingBuffer");
    staging->upload(data, size, 0);

    auto extent = texture.mip_extent(mip_level);

    BufferTextureCopyRegion region{};
    region.buffer_offset = 0;
    region.mip_level = mip_level;
    region.base_array_layer = array_layer;
    region.layer_count = 1;
    region.texture_offset = offset;
    region.texture_extent = extent;

    ImmediateContext ctx(*this);
    ctx.submit([&](CommandList& cmd) {
        // Transition to copy dest
        TextureBarrier barrier{};
        barrier.texture = &texture;
        barrier.old_state = ResourceState::Undefined;
        barrier.new_state = ResourceState::CopyDest;
        barrier.base_mip_level = mip_level;
        barrier.mip_level_count = 1;
        cmd.barrier(barrier);

        cmd.copy_buffer_to_texture(*staging, texture, {&region, 1});

        // Transition to shader resource
        barrier.old_state = ResourceState::CopyDest;
        barrier.new_state = ResourceState::ShaderResource;
        cmd.barrier(barrier);
    });
}

void VulkanDevice::generate_mipmaps(Texture& texture) {
    // TODO: Implement mipmap generation using vkCmdBlitImage
    HZ_LOG_WARN("generate_mipmaps not yet implemented");
}

} // namespace hz::rhi::vk

// ============================================================================
// Static Device Factory (in hz::rhi namespace)
// ============================================================================

namespace hz::rhi {

std::unique_ptr<Device> Device::create(const DeviceDesc& desc) {
    // Check if Vulkan is requested or auto
    if (desc.preferred_backend == Backend::Vulkan || desc.preferred_backend == Backend::Auto) {
        auto device = std::make_unique<vk::VulkanDevice>(desc);
        if (device->device() != VK_NULL_HANDLE) {
            return device;
        }
    }

    HZ_LOG_ERROR("Failed to create RHI device");
    return nullptr;
}

} // namespace hz::rhi
