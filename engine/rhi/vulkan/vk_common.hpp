#pragma once

/**
 * @file vk_common.hpp
 * @brief Common Vulkan includes, macros, and utilities
 *
 * This file sets up Vulkan headers via volk (meta-loader) and provides
 * common utilities for error checking and type conversions.
 */

// Use volk for Vulkan function loading (avoids linking issues)
#define VK_NO_PROTOTYPES
#include <volk.h>

// VMA for memory allocation
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#include "../rhi_types.hpp"
#include "engine/core/log.hpp"

#include <string>

#include <vk_mem_alloc.h>

namespace hz::rhi::vk {

// ============================================================================
// Vulkan Error Handling
// ============================================================================

/**
 * @brief Convert VkResult to string for debugging
 */
[[nodiscard]] inline const char* vk_result_string(VkResult result) noexcept {
    switch (result) {
    case VK_SUCCESS:
        return "VK_SUCCESS";
    case VK_NOT_READY:
        return "VK_NOT_READY";
    case VK_TIMEOUT:
        return "VK_TIMEOUT";
    case VK_EVENT_SET:
        return "VK_EVENT_SET";
    case VK_EVENT_RESET:
        return "VK_EVENT_RESET";
    case VK_INCOMPLETE:
        return "VK_INCOMPLETE";
    case VK_ERROR_OUT_OF_HOST_MEMORY:
        return "VK_ERROR_OUT_OF_HOST_MEMORY";
    case VK_ERROR_OUT_OF_DEVICE_MEMORY:
        return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
    case VK_ERROR_INITIALIZATION_FAILED:
        return "VK_ERROR_INITIALIZATION_FAILED";
    case VK_ERROR_DEVICE_LOST:
        return "VK_ERROR_DEVICE_LOST";
    case VK_ERROR_MEMORY_MAP_FAILED:
        return "VK_ERROR_MEMORY_MAP_FAILED";
    case VK_ERROR_LAYER_NOT_PRESENT:
        return "VK_ERROR_LAYER_NOT_PRESENT";
    case VK_ERROR_EXTENSION_NOT_PRESENT:
        return "VK_ERROR_EXTENSION_NOT_PRESENT";
    case VK_ERROR_FEATURE_NOT_PRESENT:
        return "VK_ERROR_FEATURE_NOT_PRESENT";
    case VK_ERROR_INCOMPATIBLE_DRIVER:
        return "VK_ERROR_INCOMPATIBLE_DRIVER";
    case VK_ERROR_TOO_MANY_OBJECTS:
        return "VK_ERROR_TOO_MANY_OBJECTS";
    case VK_ERROR_FORMAT_NOT_SUPPORTED:
        return "VK_ERROR_FORMAT_NOT_SUPPORTED";
    case VK_ERROR_FRAGMENTED_POOL:
        return "VK_ERROR_FRAGMENTED_POOL";
    case VK_ERROR_UNKNOWN:
        return "VK_ERROR_UNKNOWN";
    case VK_ERROR_OUT_OF_POOL_MEMORY:
        return "VK_ERROR_OUT_OF_POOL_MEMORY";
    case VK_ERROR_INVALID_EXTERNAL_HANDLE:
        return "VK_ERROR_INVALID_EXTERNAL_HANDLE";
    case VK_ERROR_FRAGMENTATION:
        return "VK_ERROR_FRAGMENTATION";
    case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS:
        return "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS";
    case VK_ERROR_SURFACE_LOST_KHR:
        return "VK_ERROR_SURFACE_LOST_KHR";
    case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
        return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
    case VK_SUBOPTIMAL_KHR:
        return "VK_SUBOPTIMAL_KHR";
    case VK_ERROR_OUT_OF_DATE_KHR:
        return "VK_ERROR_OUT_OF_DATE_KHR";
    case VK_ERROR_VALIDATION_FAILED_EXT:
        return "VK_ERROR_VALIDATION_FAILED_EXT";
    default:
        return "VK_ERROR_UNKNOWN";
    }
}

/**
 * @brief Check VkResult and log error if failed
 * @return true if successful, false otherwise
 */
[[nodiscard]] inline bool vk_check(VkResult result, const char* operation = nullptr) noexcept {
    if (result == VK_SUCCESS) {
        return true;
    }

    if (operation) {
        HZ_LOG_ERROR("Vulkan error in {}: {} ({})", operation, vk_result_string(result),
                     static_cast<int>(result));
    } else {
        HZ_LOG_ERROR("Vulkan error: {} ({})", vk_result_string(result), static_cast<int>(result));
    }
    return false;
}

/**
 * @brief Check VkResult and abort if failed (for critical operations)
 */
inline void vk_check_fatal(VkResult result, const char* operation) {
    if (result != VK_SUCCESS) {
        HZ_LOG_CRITICAL("Fatal Vulkan error in {}: {} ({})", operation, vk_result_string(result),
                        static_cast<int>(result));
        std::abort();
    }
}

// Convenience macros
#define VK_CHECK(expr) ::hz::rhi::vk::vk_check(expr, #expr)
#define VK_CHECK_FATAL(expr) ::hz::rhi::vk::vk_check_fatal(expr, #expr)

// ============================================================================
// Format Conversion
// ============================================================================

/**
 * @brief Convert RHI Format to VkFormat
 */
[[nodiscard]] constexpr VkFormat to_vk_format(Format format) noexcept {
    switch (format) {
    case Format::Unknown:
        return VK_FORMAT_UNDEFINED;

    // 8-bit
    case Format::R8_UNORM:
        return VK_FORMAT_R8_UNORM;
    case Format::R8_SNORM:
        return VK_FORMAT_R8_SNORM;
    case Format::R8_UINT:
        return VK_FORMAT_R8_UINT;
    case Format::R8_SINT:
        return VK_FORMAT_R8_SINT;
    case Format::RG8_UNORM:
        return VK_FORMAT_R8G8_UNORM;
    case Format::RG8_SNORM:
        return VK_FORMAT_R8G8_SNORM;
    case Format::RG8_UINT:
        return VK_FORMAT_R8G8_UINT;
    case Format::RG8_SINT:
        return VK_FORMAT_R8G8_SINT;
    case Format::RGBA8_UNORM:
        return VK_FORMAT_R8G8B8A8_UNORM;
    case Format::RGBA8_SNORM:
        return VK_FORMAT_R8G8B8A8_SNORM;
    case Format::RGBA8_UINT:
        return VK_FORMAT_R8G8B8A8_UINT;
    case Format::RGBA8_SINT:
        return VK_FORMAT_R8G8B8A8_SINT;
    case Format::RGBA8_SRGB:
        return VK_FORMAT_R8G8B8A8_SRGB;
    case Format::BGRA8_UNORM:
        return VK_FORMAT_B8G8R8A8_UNORM;
    case Format::BGRA8_SRGB:
        return VK_FORMAT_B8G8R8A8_SRGB;

    // 16-bit
    case Format::R16_UNORM:
        return VK_FORMAT_R16_UNORM;
    case Format::R16_SNORM:
        return VK_FORMAT_R16_SNORM;
    case Format::R16_UINT:
        return VK_FORMAT_R16_UINT;
    case Format::R16_SINT:
        return VK_FORMAT_R16_SINT;
    case Format::R16_FLOAT:
        return VK_FORMAT_R16_SFLOAT;
    case Format::RG16_UNORM:
        return VK_FORMAT_R16G16_UNORM;
    case Format::RG16_SNORM:
        return VK_FORMAT_R16G16_SNORM;
    case Format::RG16_UINT:
        return VK_FORMAT_R16G16_UINT;
    case Format::RG16_SINT:
        return VK_FORMAT_R16G16_SINT;
    case Format::RG16_FLOAT:
        return VK_FORMAT_R16G16_SFLOAT;
    case Format::RGBA16_UNORM:
        return VK_FORMAT_R16G16B16A16_UNORM;
    case Format::RGBA16_SNORM:
        return VK_FORMAT_R16G16B16A16_SNORM;
    case Format::RGBA16_UINT:
        return VK_FORMAT_R16G16B16A16_UINT;
    case Format::RGBA16_SINT:
        return VK_FORMAT_R16G16B16A16_SINT;
    case Format::RGBA16_FLOAT:
        return VK_FORMAT_R16G16B16A16_SFLOAT;

    // 32-bit
    case Format::R32_UINT:
        return VK_FORMAT_R32_UINT;
    case Format::R32_SINT:
        return VK_FORMAT_R32_SINT;
    case Format::R32_FLOAT:
        return VK_FORMAT_R32_SFLOAT;
    case Format::RG32_UINT:
        return VK_FORMAT_R32G32_UINT;
    case Format::RG32_SINT:
        return VK_FORMAT_R32G32_SINT;
    case Format::RG32_FLOAT:
        return VK_FORMAT_R32G32_SFLOAT;
    case Format::RGB32_UINT:
        return VK_FORMAT_R32G32B32_UINT;
    case Format::RGB32_SINT:
        return VK_FORMAT_R32G32B32_SINT;
    case Format::RGB32_FLOAT:
        return VK_FORMAT_R32G32B32_SFLOAT;
    case Format::RGBA32_UINT:
        return VK_FORMAT_R32G32B32A32_UINT;
    case Format::RGBA32_SINT:
        return VK_FORMAT_R32G32B32A32_SINT;
    case Format::RGBA32_FLOAT:
        return VK_FORMAT_R32G32B32A32_SFLOAT;

    // Packed
    case Format::R10G10B10A2_UNORM:
        return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
    case Format::R10G10B10A2_UINT:
        return VK_FORMAT_A2B10G10R10_UINT_PACK32;
    case Format::R11G11B10_FLOAT:
        return VK_FORMAT_B10G11R11_UFLOAT_PACK32;

    // Depth/Stencil
    case Format::D16_UNORM:
        return VK_FORMAT_D16_UNORM;
    case Format::D24_UNORM_S8_UINT:
        return VK_FORMAT_D24_UNORM_S8_UINT;
    case Format::D32_FLOAT:
        return VK_FORMAT_D32_SFLOAT;
    case Format::D32_FLOAT_S8_UINT:
        return VK_FORMAT_D32_SFLOAT_S8_UINT;

    // Compressed BC
    case Format::BC1_UNORM:
        return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
    case Format::BC1_SRGB:
        return VK_FORMAT_BC1_RGBA_SRGB_BLOCK;
    case Format::BC2_UNORM:
        return VK_FORMAT_BC2_UNORM_BLOCK;
    case Format::BC2_SRGB:
        return VK_FORMAT_BC2_SRGB_BLOCK;
    case Format::BC3_UNORM:
        return VK_FORMAT_BC3_UNORM_BLOCK;
    case Format::BC3_SRGB:
        return VK_FORMAT_BC3_SRGB_BLOCK;
    case Format::BC4_UNORM:
        return VK_FORMAT_BC4_UNORM_BLOCK;
    case Format::BC4_SNORM:
        return VK_FORMAT_BC4_SNORM_BLOCK;
    case Format::BC5_UNORM:
        return VK_FORMAT_BC5_UNORM_BLOCK;
    case Format::BC5_SNORM:
        return VK_FORMAT_BC5_SNORM_BLOCK;
    case Format::BC6H_UFLOAT:
        return VK_FORMAT_BC6H_UFLOAT_BLOCK;
    case Format::BC6H_SFLOAT:
        return VK_FORMAT_BC6H_SFLOAT_BLOCK;
    case Format::BC7_UNORM:
        return VK_FORMAT_BC7_UNORM_BLOCK;
    case Format::BC7_SRGB:
        return VK_FORMAT_BC7_SRGB_BLOCK;

    default:
        return VK_FORMAT_UNDEFINED;
    }
}

/**
 * @brief Convert VkFormat to RHI Format
 */
[[nodiscard]] constexpr Format from_vk_format(VkFormat format) noexcept {
    switch (format) {
    case VK_FORMAT_R8_UNORM:
        return Format::R8_UNORM;
    case VK_FORMAT_R8G8B8A8_UNORM:
        return Format::RGBA8_UNORM;
    case VK_FORMAT_R8G8B8A8_SRGB:
        return Format::RGBA8_SRGB;
    case VK_FORMAT_B8G8R8A8_UNORM:
        return Format::BGRA8_UNORM;
    case VK_FORMAT_B8G8R8A8_SRGB:
        return Format::BGRA8_SRGB;
    case VK_FORMAT_R16G16B16A16_SFLOAT:
        return Format::RGBA16_FLOAT;
    case VK_FORMAT_R32G32B32A32_SFLOAT:
        return Format::RGBA32_FLOAT;
    case VK_FORMAT_D32_SFLOAT:
        return Format::D32_FLOAT;
    case VK_FORMAT_D24_UNORM_S8_UINT:
        return Format::D24_UNORM_S8_UINT;
    // Add more as needed
    default:
        return Format::Unknown;
    }
}

// ============================================================================
// Enum Conversions
// ============================================================================

[[nodiscard]] constexpr VkFilter to_vk_filter(Filter filter) noexcept {
    switch (filter) {
    case Filter::Nearest:
        return VK_FILTER_NEAREST;
    case Filter::Linear:
        return VK_FILTER_LINEAR;
    }
    return VK_FILTER_LINEAR;
}

[[nodiscard]] constexpr VkSamplerMipmapMode to_vk_mipmap_mode(MipmapMode mode) noexcept {
    switch (mode) {
    case MipmapMode::Nearest:
        return VK_SAMPLER_MIPMAP_MODE_NEAREST;
    case MipmapMode::Linear:
        return VK_SAMPLER_MIPMAP_MODE_LINEAR;
    }
    return VK_SAMPLER_MIPMAP_MODE_LINEAR;
}

[[nodiscard]] constexpr VkSamplerAddressMode to_vk_address_mode(AddressMode mode) noexcept {
    switch (mode) {
    case AddressMode::Repeat:
        return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    case AddressMode::MirroredRepeat:
        return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    case AddressMode::ClampToEdge:
        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    case AddressMode::ClampToBorder:
        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    }
    return VK_SAMPLER_ADDRESS_MODE_REPEAT;
}

[[nodiscard]] constexpr VkBorderColor to_vk_border_color(BorderColor color) noexcept {
    switch (color) {
    case BorderColor::TransparentBlack:
        return VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
    case BorderColor::OpaqueBlack:
        return VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    case BorderColor::OpaqueWhite:
        return VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    }
    return VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
}

[[nodiscard]] constexpr VkCompareOp to_vk_compare_op(CompareOp op) noexcept {
    switch (op) {
    case CompareOp::Never:
        return VK_COMPARE_OP_NEVER;
    case CompareOp::Less:
        return VK_COMPARE_OP_LESS;
    case CompareOp::Equal:
        return VK_COMPARE_OP_EQUAL;
    case CompareOp::LessOrEqual:
        return VK_COMPARE_OP_LESS_OR_EQUAL;
    case CompareOp::Greater:
        return VK_COMPARE_OP_GREATER;
    case CompareOp::NotEqual:
        return VK_COMPARE_OP_NOT_EQUAL;
    case CompareOp::GreaterOrEqual:
        return VK_COMPARE_OP_GREATER_OR_EQUAL;
    case CompareOp::Always:
        return VK_COMPARE_OP_ALWAYS;
    }
    return VK_COMPARE_OP_ALWAYS;
}

[[nodiscard]] constexpr VkPrimitiveTopology to_vk_topology(PrimitiveTopology topology) noexcept {
    switch (topology) {
    case PrimitiveTopology::PointList:
        return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    case PrimitiveTopology::LineList:
        return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    case PrimitiveTopology::LineStrip:
        return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
    case PrimitiveTopology::TriangleList:
        return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    case PrimitiveTopology::TriangleStrip:
        return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    case PrimitiveTopology::TriangleFan:
        return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
    case PrimitiveTopology::PatchList:
        return VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
    }
    return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
}

[[nodiscard]] constexpr VkPolygonMode to_vk_polygon_mode(PolygonMode mode) noexcept {
    switch (mode) {
    case PolygonMode::Fill:
        return VK_POLYGON_MODE_FILL;
    case PolygonMode::Line:
        return VK_POLYGON_MODE_LINE;
    case PolygonMode::Point:
        return VK_POLYGON_MODE_POINT;
    }
    return VK_POLYGON_MODE_FILL;
}

[[nodiscard]] constexpr VkCullModeFlags to_vk_cull_mode(CullMode mode) noexcept {
    switch (mode) {
    case CullMode::None:
        return VK_CULL_MODE_NONE;
    case CullMode::Front:
        return VK_CULL_MODE_FRONT_BIT;
    case CullMode::Back:
        return VK_CULL_MODE_BACK_BIT;
    case CullMode::FrontAndBack:
        return VK_CULL_MODE_FRONT_AND_BACK;
    }
    return VK_CULL_MODE_BACK_BIT;
}

[[nodiscard]] constexpr VkFrontFace to_vk_front_face(FrontFace face) noexcept {
    switch (face) {
    case FrontFace::CounterClockwise:
        return VK_FRONT_FACE_COUNTER_CLOCKWISE;
    case FrontFace::Clockwise:
        return VK_FRONT_FACE_CLOCKWISE;
    }
    return VK_FRONT_FACE_COUNTER_CLOCKWISE;
}

[[nodiscard]] constexpr VkBlendFactor to_vk_blend_factor(BlendFactor factor) noexcept {
    switch (factor) {
    case BlendFactor::Zero:
        return VK_BLEND_FACTOR_ZERO;
    case BlendFactor::One:
        return VK_BLEND_FACTOR_ONE;
    case BlendFactor::SrcColor:
        return VK_BLEND_FACTOR_SRC_COLOR;
    case BlendFactor::OneMinusSrcColor:
        return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
    case BlendFactor::DstColor:
        return VK_BLEND_FACTOR_DST_COLOR;
    case BlendFactor::OneMinusDstColor:
        return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
    case BlendFactor::SrcAlpha:
        return VK_BLEND_FACTOR_SRC_ALPHA;
    case BlendFactor::OneMinusSrcAlpha:
        return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    case BlendFactor::DstAlpha:
        return VK_BLEND_FACTOR_DST_ALPHA;
    case BlendFactor::OneMinusDstAlpha:
        return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
    case BlendFactor::ConstantColor:
        return VK_BLEND_FACTOR_CONSTANT_COLOR;
    case BlendFactor::OneMinusConstantColor:
        return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
    case BlendFactor::ConstantAlpha:
        return VK_BLEND_FACTOR_CONSTANT_ALPHA;
    case BlendFactor::OneMinusConstantAlpha:
        return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
    case BlendFactor::SrcAlphaSaturate:
        return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
    case BlendFactor::Src1Color:
        return VK_BLEND_FACTOR_SRC1_COLOR;
    case BlendFactor::OneMinusSrc1Color:
        return VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR;
    case BlendFactor::Src1Alpha:
        return VK_BLEND_FACTOR_SRC1_ALPHA;
    case BlendFactor::OneMinusSrc1Alpha:
        return VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA;
    }
    return VK_BLEND_FACTOR_ONE;
}

[[nodiscard]] constexpr VkBlendOp to_vk_blend_op(BlendOp op) noexcept {
    switch (op) {
    case BlendOp::Add:
        return VK_BLEND_OP_ADD;
    case BlendOp::Subtract:
        return VK_BLEND_OP_SUBTRACT;
    case BlendOp::ReverseSubtract:
        return VK_BLEND_OP_REVERSE_SUBTRACT;
    case BlendOp::Min:
        return VK_BLEND_OP_MIN;
    case BlendOp::Max:
        return VK_BLEND_OP_MAX;
    }
    return VK_BLEND_OP_ADD;
}

[[nodiscard]] constexpr VkColorComponentFlags to_vk_color_write_mask(ColorWriteMask mask) noexcept {
    VkColorComponentFlags flags = 0;
    if (has_flag(mask, ColorWriteMask::R))
        flags |= VK_COLOR_COMPONENT_R_BIT;
    if (has_flag(mask, ColorWriteMask::G))
        flags |= VK_COLOR_COMPONENT_G_BIT;
    if (has_flag(mask, ColorWriteMask::B))
        flags |= VK_COLOR_COMPONENT_B_BIT;
    if (has_flag(mask, ColorWriteMask::A))
        flags |= VK_COLOR_COMPONENT_A_BIT;
    return flags;
}

[[nodiscard]] constexpr VkStencilOp to_vk_stencil_op(StencilOp op) noexcept {
    switch (op) {
    case StencilOp::Keep:
        return VK_STENCIL_OP_KEEP;
    case StencilOp::Zero:
        return VK_STENCIL_OP_ZERO;
    case StencilOp::Replace:
        return VK_STENCIL_OP_REPLACE;
    case StencilOp::IncrementClamp:
        return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
    case StencilOp::DecrementClamp:
        return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
    case StencilOp::Invert:
        return VK_STENCIL_OP_INVERT;
    case StencilOp::IncrementWrap:
        return VK_STENCIL_OP_INCREMENT_AND_WRAP;
    case StencilOp::DecrementWrap:
        return VK_STENCIL_OP_DECREMENT_AND_WRAP;
    }
    return VK_STENCIL_OP_KEEP;
}

[[nodiscard]] constexpr VkShaderStageFlags to_vk_shader_stages(ShaderStage stages) noexcept {
    VkShaderStageFlags flags = 0;
    if (has_flag(stages, ShaderStage::Vertex))
        flags |= VK_SHADER_STAGE_VERTEX_BIT;
    if (has_flag(stages, ShaderStage::Fragment))
        flags |= VK_SHADER_STAGE_FRAGMENT_BIT;
    if (has_flag(stages, ShaderStage::Geometry))
        flags |= VK_SHADER_STAGE_GEOMETRY_BIT;
    if (has_flag(stages, ShaderStage::TessellationControl))
        flags |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
    if (has_flag(stages, ShaderStage::TessellationEvaluation))
        flags |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
    if (has_flag(stages, ShaderStage::Compute))
        flags |= VK_SHADER_STAGE_COMPUTE_BIT;
    return flags;
}

[[nodiscard]] constexpr VkShaderStageFlagBits to_vk_shader_stage(ShaderStage stage) noexcept {
    switch (stage) {
    case ShaderStage::Vertex:
        return VK_SHADER_STAGE_VERTEX_BIT;
    case ShaderStage::Fragment:
        return VK_SHADER_STAGE_FRAGMENT_BIT;
    case ShaderStage::Geometry:
        return VK_SHADER_STAGE_GEOMETRY_BIT;
    case ShaderStage::TessellationControl:
        return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
    case ShaderStage::TessellationEvaluation:
        return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
    case ShaderStage::Compute:
        return VK_SHADER_STAGE_COMPUTE_BIT;
    default:
        return VK_SHADER_STAGE_ALL;
    }
}

[[nodiscard]] constexpr VkDescriptorType to_vk_descriptor_type(DescriptorType type) noexcept {
    switch (type) {
    case DescriptorType::Sampler:
        return VK_DESCRIPTOR_TYPE_SAMPLER;
    case DescriptorType::CombinedImageSampler:
        return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    case DescriptorType::SampledImage:
        return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    case DescriptorType::StorageImage:
        return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    case DescriptorType::UniformBuffer:
        return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    case DescriptorType::StorageBuffer:
        return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    case DescriptorType::UniformBufferDynamic:
        return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    case DescriptorType::StorageBufferDynamic:
        return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
    case DescriptorType::InputAttachment:
        return VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
    }
    return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
}

[[nodiscard]] constexpr VkAttachmentLoadOp to_vk_load_op(LoadOp op) noexcept {
    switch (op) {
    case LoadOp::Load:
        return VK_ATTACHMENT_LOAD_OP_LOAD;
    case LoadOp::Clear:
        return VK_ATTACHMENT_LOAD_OP_CLEAR;
    case LoadOp::DontCare:
        return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    }
    return VK_ATTACHMENT_LOAD_OP_CLEAR;
}

[[nodiscard]] constexpr VkAttachmentStoreOp to_vk_store_op(StoreOp op) noexcept {
    switch (op) {
    case StoreOp::Store:
        return VK_ATTACHMENT_STORE_OP_STORE;
    case StoreOp::DontCare:
        return VK_ATTACHMENT_STORE_OP_DONT_CARE;
    }
    return VK_ATTACHMENT_STORE_OP_STORE;
}

[[nodiscard]] constexpr VkImageLayout to_vk_image_layout(ResourceState state) noexcept {
    switch (state) {
    case ResourceState::Undefined:
        return VK_IMAGE_LAYOUT_UNDEFINED;
    case ResourceState::Common:
        return VK_IMAGE_LAYOUT_GENERAL;
    case ResourceState::ShaderResource:
        return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    case ResourceState::UnorderedAccess:
        return VK_IMAGE_LAYOUT_GENERAL;
    case ResourceState::RenderTarget:
        return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    case ResourceState::DepthWrite:
        return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    case ResourceState::DepthRead:
        return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    case ResourceState::CopySource:
        return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    case ResourceState::CopyDest:
        return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    case ResourceState::Present:
        return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    default:
        return VK_IMAGE_LAYOUT_GENERAL;
    }
}

[[nodiscard]] constexpr VkIndexType to_vk_index_type(IndexType type) noexcept {
    switch (type) {
    case IndexType::Uint16:
        return VK_INDEX_TYPE_UINT16;
    case IndexType::Uint32:
        return VK_INDEX_TYPE_UINT32;
    }
    return VK_INDEX_TYPE_UINT32;
}

[[nodiscard]] constexpr VkImageType to_vk_image_type(TextureType type) noexcept {
    switch (type) {
    case TextureType::Texture1D:
    case TextureType::Texture1DArray:
        return VK_IMAGE_TYPE_1D;
    case TextureType::Texture2D:
    case TextureType::Texture2DArray:
    case TextureType::TextureCube:
    case TextureType::TextureCubeArray:
        return VK_IMAGE_TYPE_2D;
    case TextureType::Texture3D:
        return VK_IMAGE_TYPE_3D;
    }
    return VK_IMAGE_TYPE_2D;
}

[[nodiscard]] constexpr VkImageViewType to_vk_image_view_type(TextureType type) noexcept {
    switch (type) {
    case TextureType::Texture1D:
        return VK_IMAGE_VIEW_TYPE_1D;
    case TextureType::Texture1DArray:
        return VK_IMAGE_VIEW_TYPE_1D_ARRAY;
    case TextureType::Texture2D:
        return VK_IMAGE_VIEW_TYPE_2D;
    case TextureType::Texture2DArray:
        return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    case TextureType::Texture3D:
        return VK_IMAGE_VIEW_TYPE_3D;
    case TextureType::TextureCube:
        return VK_IMAGE_VIEW_TYPE_CUBE;
    case TextureType::TextureCubeArray:
        return VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
    }
    return VK_IMAGE_VIEW_TYPE_2D;
}

[[nodiscard]] constexpr VkImageUsageFlags to_vk_image_usage(TextureUsage usage) noexcept {
    VkImageUsageFlags flags = 0;
    if (has_flag(usage, TextureUsage::Sampled))
        flags |= VK_IMAGE_USAGE_SAMPLED_BIT;
    if (has_flag(usage, TextureUsage::Storage))
        flags |= VK_IMAGE_USAGE_STORAGE_BIT;
    if (has_flag(usage, TextureUsage::RenderTarget))
        flags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (has_flag(usage, TextureUsage::DepthStencil))
        flags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    if (has_flag(usage, TextureUsage::TransferSrc))
        flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    if (has_flag(usage, TextureUsage::TransferDst))
        flags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if (has_flag(usage, TextureUsage::InputAttachment))
        flags |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
    return flags;
}

[[nodiscard]] constexpr VkBufferUsageFlags to_vk_buffer_usage(BufferUsage usage) noexcept {
    VkBufferUsageFlags flags = 0;
    if (has_flag(usage, BufferUsage::VertexBuffer))
        flags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    if (has_flag(usage, BufferUsage::IndexBuffer))
        flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    if (has_flag(usage, BufferUsage::UniformBuffer))
        flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    if (has_flag(usage, BufferUsage::StorageBuffer))
        flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    if (has_flag(usage, BufferUsage::IndirectBuffer))
        flags |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
    if (has_flag(usage, BufferUsage::TransferSrc))
        flags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    if (has_flag(usage, BufferUsage::TransferDst))
        flags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    return flags;
}

[[nodiscard]] constexpr VmaMemoryUsage to_vma_memory_usage(MemoryUsage usage) noexcept {
    switch (usage) {
    case MemoryUsage::GPU_Only:
        return VMA_MEMORY_USAGE_GPU_ONLY;
    case MemoryUsage::CPU_To_GPU:
        return VMA_MEMORY_USAGE_CPU_TO_GPU;
    case MemoryUsage::GPU_To_CPU:
        return VMA_MEMORY_USAGE_GPU_TO_CPU;
    case MemoryUsage::CPU_Only:
        return VMA_MEMORY_USAGE_CPU_ONLY;
    }
    return VMA_MEMORY_USAGE_GPU_ONLY;
}

[[nodiscard]] constexpr VkImageAspectFlags get_image_aspect(Format format) noexcept {
    if (is_depth_format(format)) {
        VkImageAspectFlags aspects = VK_IMAGE_ASPECT_DEPTH_BIT;
        if (has_stencil(format)) {
            aspects |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }
        return aspects;
    }
    return VK_IMAGE_ASPECT_COLOR_BIT;
}

// ============================================================================
// Pipeline Barrier Helpers
// ============================================================================

[[nodiscard]] constexpr VkPipelineStageFlags to_vk_pipeline_stage(ResourceState state) noexcept {
    switch (state) {
    case ResourceState::Undefined:
        return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    case ResourceState::VertexBuffer:
    case ResourceState::IndexBuffer:
        return VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
    case ResourceState::UniformBuffer:
    case ResourceState::ShaderResource:
        return VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
               VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    case ResourceState::UnorderedAccess:
        return VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    case ResourceState::RenderTarget:
        return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    case ResourceState::DepthWrite:
    case ResourceState::DepthRead:
        return VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
               VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    case ResourceState::IndirectArgument:
        return VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
    case ResourceState::CopySource:
    case ResourceState::CopyDest:
        return VK_PIPELINE_STAGE_TRANSFER_BIT;
    case ResourceState::Present:
        return VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    default:
        return VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    }
}

[[nodiscard]] constexpr VkAccessFlags to_vk_access_flags(ResourceState state) noexcept {
    switch (state) {
    case ResourceState::Undefined:
        return 0;
    case ResourceState::VertexBuffer:
        return VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
    case ResourceState::IndexBuffer:
        return VK_ACCESS_INDEX_READ_BIT;
    case ResourceState::UniformBuffer:
        return VK_ACCESS_UNIFORM_READ_BIT;
    case ResourceState::ShaderResource:
        return VK_ACCESS_SHADER_READ_BIT;
    case ResourceState::UnorderedAccess:
        return VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    case ResourceState::RenderTarget:
        return VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    case ResourceState::DepthWrite:
        return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
               VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    case ResourceState::DepthRead:
        return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
    case ResourceState::IndirectArgument:
        return VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
    case ResourceState::CopySource:
        return VK_ACCESS_TRANSFER_READ_BIT;
    case ResourceState::CopyDest:
        return VK_ACCESS_TRANSFER_WRITE_BIT;
    case ResourceState::Present:
        return 0;
    default:
        return VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    }
}

} // namespace hz::rhi::vk
