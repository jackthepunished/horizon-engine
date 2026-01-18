#pragma once

/**
 * @file rhi_types.hpp
 * @brief Core RHI type definitions, enums, and flags
 *
 * This file defines all fundamental types used throughout the Render Hardware Interface.
 * It provides a unified abstraction over Vulkan, DirectX 12, and OpenGL concepts.
 */

#include "engine/core/types.hpp"

#include <array>
#include <type_traits>
#include <variant>

#include <glm/glm.hpp>

namespace hz::rhi {

// ============================================================================
// Forward Declarations
// ============================================================================

class Device;
class CommandList;
class Buffer;
class Texture;
class TextureView;
class Sampler;
class ShaderModule;
class Pipeline;
class PipelineLayout;
class RenderPass;
class Framebuffer;
class DescriptorSetLayout;
class DescriptorSet;
class Fence;
class Semaphore;
class Swapchain;

// ============================================================================
// Backend Selection
// ============================================================================

/**
 * @brief Available graphics API backends
 */
enum class Backend : u8 {
    Vulkan, ///< Vulkan 1.2+ (cross-platform, primary target)
    D3D12,  ///< DirectX 12 (Windows only)
    OpenGL, ///< OpenGL 4.5+ (fallback for older hardware)
    Auto    ///< Automatically select best available backend
};

// ============================================================================
// Resource Formats
// ============================================================================

/**
 * @brief Unified texture and vertex format enumeration
 *
 * Naming convention: {Components}_{BitDepth}_{Type}[_{ColorSpace}]
 * - Components: R, RG, RGB, RGBA, BGR, BGRA, D (depth), S (stencil)
 * - BitDepth: 8, 16, 32 per component
 * - Type: UNORM, SNORM, UINT, SINT, FLOAT, SRGB
 */
enum class Format : u32 {
    Unknown = 0,

    // ========================================================================
    // 8-bit per channel formats
    // ========================================================================
    R8_UNORM,
    R8_SNORM,
    R8_UINT,
    R8_SINT,
    RG8_UNORM,
    RG8_SNORM,
    RG8_UINT,
    RG8_SINT,
    RGBA8_UNORM,
    RGBA8_SNORM,
    RGBA8_UINT,
    RGBA8_SINT,
    RGBA8_SRGB,
    BGRA8_UNORM,
    BGRA8_SRGB,

    // ========================================================================
    // 16-bit per channel formats
    // ========================================================================
    R16_UNORM,
    R16_SNORM,
    R16_UINT,
    R16_SINT,
    R16_FLOAT,
    RG16_UNORM,
    RG16_SNORM,
    RG16_UINT,
    RG16_SINT,
    RG16_FLOAT,
    RGBA16_UNORM,
    RGBA16_SNORM,
    RGBA16_UINT,
    RGBA16_SINT,
    RGBA16_FLOAT,

    // ========================================================================
    // 32-bit per channel formats
    // ========================================================================
    R32_UINT,
    R32_SINT,
    R32_FLOAT,
    RG32_UINT,
    RG32_SINT,
    RG32_FLOAT,
    RGB32_UINT,
    RGB32_SINT,
    RGB32_FLOAT,
    RGBA32_UINT,
    RGBA32_SINT,
    RGBA32_FLOAT,

    // ========================================================================
    // Packed formats
    // ========================================================================
    R10G10B10A2_UNORM,
    R10G10B10A2_UINT,
    R11G11B10_FLOAT,

    // ========================================================================
    // Depth/Stencil formats
    // ========================================================================
    D16_UNORM,
    D24_UNORM_S8_UINT,
    D32_FLOAT,
    D32_FLOAT_S8_UINT,

    // ========================================================================
    // Compressed formats (BC/DXT for desktop)
    // ========================================================================
    BC1_UNORM, ///< DXT1 - RGB, 4:1 compression
    BC1_SRGB,
    BC2_UNORM, ///< DXT3 - RGBA with explicit alpha
    BC2_SRGB,
    BC3_UNORM, ///< DXT5 - RGBA with interpolated alpha
    BC3_SRGB,
    BC4_UNORM, ///< Single channel, 2:1 compression
    BC4_SNORM,
    BC5_UNORM, ///< Two channel (normal maps), 2:1 compression
    BC5_SNORM,
    BC6H_UFLOAT, ///< HDR RGB, unsigned
    BC6H_SFLOAT, ///< HDR RGB, signed
    BC7_UNORM,   ///< High quality RGBA
    BC7_SRGB,

    Count
};

/**
 * @brief Get the size in bytes of a single pixel/texel for the given format
 * @note Returns 0 for compressed formats (use block size instead)
 */
[[nodiscard]] constexpr u32 format_bytes_per_pixel(Format format) noexcept {
    switch (format) {
    case Format::R8_UNORM:
    case Format::R8_SNORM:
    case Format::R8_UINT:
    case Format::R8_SINT:
        return 1;
    case Format::RG8_UNORM:
    case Format::RG8_SNORM:
    case Format::RG8_UINT:
    case Format::RG8_SINT:
    case Format::R16_UNORM:
    case Format::R16_SNORM:
    case Format::R16_UINT:
    case Format::R16_SINT:
    case Format::R16_FLOAT:
    case Format::D16_UNORM:
        return 2;
    case Format::RGBA8_UNORM:
    case Format::RGBA8_SNORM:
    case Format::RGBA8_UINT:
    case Format::RGBA8_SINT:
    case Format::RGBA8_SRGB:
    case Format::BGRA8_UNORM:
    case Format::BGRA8_SRGB:
    case Format::RG16_UNORM:
    case Format::RG16_SNORM:
    case Format::RG16_UINT:
    case Format::RG16_SINT:
    case Format::RG16_FLOAT:
    case Format::R32_UINT:
    case Format::R32_SINT:
    case Format::R32_FLOAT:
    case Format::R10G10B10A2_UNORM:
    case Format::R10G10B10A2_UINT:
    case Format::R11G11B10_FLOAT:
    case Format::D24_UNORM_S8_UINT:
    case Format::D32_FLOAT:
        return 4;
    case Format::D32_FLOAT_S8_UINT:
        return 5; // 4 bytes depth + 1 byte stencil (padded in practice)
    case Format::RGBA16_UNORM:
    case Format::RGBA16_SNORM:
    case Format::RGBA16_UINT:
    case Format::RGBA16_SINT:
    case Format::RGBA16_FLOAT:
    case Format::RG32_UINT:
    case Format::RG32_SINT:
    case Format::RG32_FLOAT:
        return 8;
    case Format::RGB32_UINT:
    case Format::RGB32_SINT:
    case Format::RGB32_FLOAT:
        return 12;
    case Format::RGBA32_UINT:
    case Format::RGBA32_SINT:
    case Format::RGBA32_FLOAT:
        return 16;
    default:
        return 0; // Compressed or unknown
    }
}

/**
 * @brief Check if a format is a depth or depth-stencil format
 */
[[nodiscard]] constexpr bool is_depth_format(Format format) noexcept {
    switch (format) {
    case Format::D16_UNORM:
    case Format::D24_UNORM_S8_UINT:
    case Format::D32_FLOAT:
    case Format::D32_FLOAT_S8_UINT:
        return true;
    default:
        return false;
    }
}

/**
 * @brief Check if a format has a stencil component
 */
[[nodiscard]] constexpr bool has_stencil(Format format) noexcept {
    switch (format) {
    case Format::D24_UNORM_S8_UINT:
    case Format::D32_FLOAT_S8_UINT:
        return true;
    default:
        return false;
    }
}

/**
 * @brief Check if a format is sRGB
 */
[[nodiscard]] constexpr bool is_srgb_format(Format format) noexcept {
    switch (format) {
    case Format::RGBA8_SRGB:
    case Format::BGRA8_SRGB:
    case Format::BC1_SRGB:
    case Format::BC2_SRGB:
    case Format::BC3_SRGB:
    case Format::BC7_SRGB:
        return true;
    default:
        return false;
    }
}

/**
 * @brief Check if a format is block-compressed
 */
[[nodiscard]] constexpr bool is_compressed_format(Format format) noexcept {
    switch (format) {
    case Format::BC1_UNORM:
    case Format::BC1_SRGB:
    case Format::BC2_UNORM:
    case Format::BC2_SRGB:
    case Format::BC3_UNORM:
    case Format::BC3_SRGB:
    case Format::BC4_UNORM:
    case Format::BC4_SNORM:
    case Format::BC5_UNORM:
    case Format::BC5_SNORM:
    case Format::BC6H_UFLOAT:
    case Format::BC6H_SFLOAT:
    case Format::BC7_UNORM:
    case Format::BC7_SRGB:
        return true;
    default:
        return false;
    }
}

// ============================================================================
// Buffer Types
// ============================================================================

/**
 * @brief Buffer usage flags (can be combined)
 */
enum class BufferUsage : u32 {
    None = 0,
    VertexBuffer = 1 << 0,   ///< Can be bound as vertex buffer
    IndexBuffer = 1 << 1,    ///< Can be bound as index buffer
    UniformBuffer = 1 << 2,  ///< Constant buffer / uniform block
    StorageBuffer = 1 << 3,  ///< Shader storage buffer / UAV
    IndirectBuffer = 1 << 4, ///< Indirect draw/dispatch arguments
    TransferSrc = 1 << 5,    ///< Source for copy operations
    TransferDst = 1 << 6,    ///< Destination for copy operations
};
HZ_ENABLE_BITMASK_OPERATORS(BufferUsage)

/**
 * @brief Memory allocation strategy
 */
enum class MemoryUsage : u8 {
    GPU_Only,   ///< DEVICE_LOCAL - fast GPU access, no CPU access
    CPU_To_GPU, ///< HOST_VISIBLE | HOST_COHERENT - upload heaps
    GPU_To_CPU, ///< HOST_VISIBLE | HOST_CACHED - readback buffers
    CPU_Only,   ///< HOST_VISIBLE - staging buffers
};

// ============================================================================
// Texture Types
// ============================================================================

/**
 * @brief Texture dimensionality
 */
enum class TextureType : u8 {
    Texture1D,
    Texture2D,
    Texture3D,
    TextureCube,
    Texture1DArray,
    Texture2DArray,
    TextureCubeArray,
};

/**
 * @brief Texture usage flags (can be combined)
 */
enum class TextureUsage : u32 {
    None = 0,
    Sampled = 1 << 0,         ///< Can be sampled in shaders (SRV)
    Storage = 1 << 1,         ///< Can be written in compute shaders (UAV)
    RenderTarget = 1 << 2,    ///< Can be used as color attachment
    DepthStencil = 1 << 3,    ///< Can be used as depth-stencil attachment
    TransferSrc = 1 << 4,     ///< Source for copy operations
    TransferDst = 1 << 5,     ///< Destination for copy operations
    InputAttachment = 1 << 6, ///< Can be read as input attachment in subpass
};
HZ_ENABLE_BITMASK_OPERATORS(TextureUsage)

// ============================================================================
// Sampler Configuration
// ============================================================================

/**
 * @brief Texture filtering mode
 */
enum class Filter : u8 {
    Nearest, ///< Point sampling
    Linear,  ///< Bilinear/trilinear filtering
};

/**
 * @brief Mipmap filtering mode
 */
enum class MipmapMode : u8 {
    Nearest, ///< Select nearest mip level
    Linear,  ///< Interpolate between mip levels
};

/**
 * @brief Texture addressing/wrap mode
 */
enum class AddressMode : u8 {
    Repeat,         ///< Tile the texture
    MirroredRepeat, ///< Tile with mirroring
    ClampToEdge,    ///< Clamp to edge texel
    ClampToBorder,  ///< Use border color
};

/**
 * @brief Border color for ClampToBorder addressing
 */
enum class BorderColor : u8 {
    TransparentBlack,
    OpaqueBlack,
    OpaqueWhite,
};

// ============================================================================
// Comparison Operations
// ============================================================================

/**
 * @brief Comparison function for depth/stencil testing and shadow sampling
 */
enum class CompareOp : u8 {
    Never,
    Less,
    Equal,
    LessOrEqual,
    Greater,
    NotEqual,
    GreaterOrEqual,
    Always,
};

// ============================================================================
// Pipeline State - Primitive Assembly
// ============================================================================

/**
 * @brief Primitive topology for input assembly
 */
enum class PrimitiveTopology : u8 {
    PointList,
    LineList,
    LineStrip,
    TriangleList,
    TriangleStrip,
    TriangleFan, ///< Not supported in Vulkan/DX12 core
    PatchList,   ///< For tessellation
};

/**
 * @brief Polygon rasterization mode
 */
enum class PolygonMode : u8 {
    Fill,  ///< Filled polygons
    Line,  ///< Wireframe
    Point, ///< Points at vertices
};

/**
 * @brief Face culling mode
 */
enum class CullMode : u8 {
    None,         ///< No culling
    Front,        ///< Cull front faces
    Back,         ///< Cull back faces
    FrontAndBack, ///< Cull all faces (rare)
};

/**
 * @brief Front face winding order
 */
enum class FrontFace : u8 {
    CounterClockwise, ///< CCW vertices are front-facing
    Clockwise,        ///< CW vertices are front-facing
};

// ============================================================================
// Pipeline State - Blending
// ============================================================================

/**
 * @brief Blend factor for color/alpha blending
 */
enum class BlendFactor : u8 {
    Zero,
    One,
    SrcColor,
    OneMinusSrcColor,
    DstColor,
    OneMinusDstColor,
    SrcAlpha,
    OneMinusSrcAlpha,
    DstAlpha,
    OneMinusDstAlpha,
    ConstantColor,
    OneMinusConstantColor,
    ConstantAlpha,
    OneMinusConstantAlpha,
    SrcAlphaSaturate,
    Src1Color,
    OneMinusSrc1Color,
    Src1Alpha,
    OneMinusSrc1Alpha,
};

/**
 * @brief Blend operation
 */
enum class BlendOp : u8 {
    Add,
    Subtract,
    ReverseSubtract,
    Min,
    Max,
};

/**
 * @brief Color write mask flags
 */
enum class ColorWriteMask : u8 {
    None = 0,
    R = 1 << 0,
    G = 1 << 1,
    B = 1 << 2,
    A = 1 << 3,
    All = R | G | B | A,
};
HZ_ENABLE_BITMASK_OPERATORS(ColorWriteMask)

// ============================================================================
// Pipeline State - Depth/Stencil
// ============================================================================

/**
 * @brief Stencil operation
 */
enum class StencilOp : u8 {
    Keep,
    Zero,
    Replace,
    IncrementClamp,
    DecrementClamp,
    Invert,
    IncrementWrap,
    DecrementWrap,
};

// ============================================================================
// Shader Stages
// ============================================================================

/**
 * @brief Shader stage flags (can be combined)
 */
enum class ShaderStage : u32 {
    None = 0,
    Vertex = 1 << 0,
    TessellationControl = 1 << 1,    ///< Hull shader in DX12
    TessellationEvaluation = 1 << 2, ///< Domain shader in DX12
    Geometry = 1 << 3,
    Fragment = 1 << 4, ///< Pixel shader in DX12
    Compute = 1 << 5,

    // Mesh shading (Vulkan 1.3 / DX12 Ultimate)
    Task = 1 << 6, ///< Amplification shader in DX12
    Mesh = 1 << 7,

    // Ray tracing (if supported)
    RayGen = 1 << 8,
    AnyHit = 1 << 9,
    ClosestHit = 1 << 10,
    Miss = 1 << 11,
    Intersection = 1 << 12,
    Callable = 1 << 13,

    AllGraphics = Vertex | TessellationControl | TessellationEvaluation | Geometry | Fragment,
    All = AllGraphics | Compute | Task | Mesh,
};
HZ_ENABLE_BITMASK_OPERATORS(ShaderStage)

// ============================================================================
// Resource States (for barriers/transitions)
// ============================================================================

/**
 * @brief Resource state for synchronization barriers
 */
enum class ResourceState : u32 {
    Undefined = 0,    ///< Initial state, contents undefined
    Common,           ///< General purpose state
    VertexBuffer,     ///< Bound as vertex buffer
    IndexBuffer,      ///< Bound as index buffer
    UniformBuffer,    ///< Bound as uniform/constant buffer
    ShaderResource,   ///< Read in shader (SRV)
    UnorderedAccess,  ///< Read/write in shader (UAV)
    RenderTarget,     ///< Color attachment output
    DepthWrite,       ///< Depth-stencil write
    DepthRead,        ///< Depth-stencil read only
    IndirectArgument, ///< Indirect draw/dispatch source
    CopySource,       ///< Copy operation source
    CopyDest,         ///< Copy operation destination
    Present,          ///< Ready for presentation
};

// ============================================================================
// Load/Store Operations
// ============================================================================

/**
 * @brief Attachment load operation at render pass begin
 */
enum class LoadOp : u8 {
    Load,     ///< Preserve existing contents
    Clear,    ///< Clear to specified value
    DontCare, ///< Contents undefined (best for transient)
};

/**
 * @brief Attachment store operation at render pass end
 */
enum class StoreOp : u8 {
    Store,    ///< Preserve contents after pass
    DontCare, ///< Contents may be discarded
};

// ============================================================================
// Descriptor Types
// ============================================================================

/**
 * @brief Type of resource binding in a descriptor set
 */
enum class DescriptorType : u8 {
    Sampler,              ///< Standalone sampler
    CombinedImageSampler, ///< Texture + sampler combined (common in GLSL)
    SampledImage,         ///< Texture without sampler
    StorageImage,         ///< Read/write texture (UAV)
    UniformBuffer,        ///< Constant buffer
    StorageBuffer,        ///< Read/write buffer (UAV / SSBO)
    UniformBufferDynamic, ///< Uniform buffer with dynamic offset
    StorageBufferDynamic, ///< Storage buffer with dynamic offset
    InputAttachment,      ///< Subpass input (Vulkan-specific)
};

// ============================================================================
// Queue Types
// ============================================================================

/**
 * @brief Command queue type
 */
enum class QueueType : u8 {
    Graphics, ///< Full graphics + compute + transfer
    Compute,  ///< Compute + transfer only (async compute)
    Transfer, ///< Transfer/copy only (DMA engine)
};

// ============================================================================
// Index Type
// ============================================================================

/**
 * @brief Index buffer element type
 */
enum class IndexType : u8 {
    Uint16,
    Uint32,
};

// ============================================================================
// Vertex Input Rate
// ============================================================================

/**
 * @brief Vertex attribute input rate
 */
enum class VertexInputRate : u8 {
    Vertex,   ///< Advance per vertex
    Instance, ///< Advance per instance
};

// ============================================================================
// Clear Values
// ============================================================================

/**
 * @brief Clear value for color attachments
 */
struct ClearColor {
    f32 r{0.0f};
    f32 g{0.0f};
    f32 b{0.0f};
    f32 a{1.0f};

    constexpr ClearColor() = default;
    constexpr ClearColor(f32 r_, f32 g_, f32 b_, f32 a_ = 1.0f) : r(r_), g(g_), b(b_), a(a_) {}
    constexpr explicit ClearColor(const glm::vec4& v) : r(v.r), g(v.g), b(v.b), a(v.a) {}

    [[nodiscard]] static constexpr ClearColor black() { return {0.0f, 0.0f, 0.0f, 1.0f}; }
    [[nodiscard]] static constexpr ClearColor white() { return {1.0f, 1.0f, 1.0f, 1.0f}; }
    [[nodiscard]] static constexpr ClearColor transparent() { return {0.0f, 0.0f, 0.0f, 0.0f}; }
    [[nodiscard]] static constexpr ClearColor cornflower_blue() {
        return {0.392f, 0.584f, 0.929f, 1.0f};
    }
};

/**
 * @brief Clear value for depth-stencil attachments
 */
struct ClearDepthStencil {
    f32 depth{1.0f};
    u8 stencil{0};

    constexpr ClearDepthStencil() = default;
    constexpr ClearDepthStencil(f32 d, u8 s = 0) : depth(d), stencil(s) {}
};

/**
 * @brief Union of clear values for any attachment type
 * Uses std::variant to allow runtime type discrimination
 */
using ClearValue = std::variant<ClearColor, ClearDepthStencil>;

// ============================================================================
// Viewport and Scissor
// ============================================================================

/**
 * @brief Viewport specification
 */
struct Viewport {
    f32 x{0.0f};
    f32 y{0.0f};
    f32 width{0.0f};
    f32 height{0.0f};
    f32 min_depth{0.0f};
    f32 max_depth{1.0f};

    constexpr Viewport() = default;
    constexpr Viewport(f32 w, f32 h) : width(w), height(h) {}
    constexpr Viewport(f32 x_, f32 y_, f32 w, f32 h, f32 mind = 0.0f, f32 maxd = 1.0f)
        : x(x_), y(y_), width(w), height(h), min_depth(mind), max_depth(maxd) {}
};

/**
 * @brief Scissor rectangle
 */
struct Scissor {
    i32 x{0};
    i32 y{0};
    u32 width{0};
    u32 height{0};

    constexpr Scissor() = default;
    constexpr Scissor(u32 w, u32 h) : width(w), height(h) {}
    constexpr Scissor(i32 x_, i32 y_, u32 w, u32 h) : x(x_), y(y_), width(w), height(h) {}
};

// ============================================================================
// Extent Types
// ============================================================================

/**
 * @brief 2D extent (width, height)
 */
struct Extent2D {
    u32 width{0};
    u32 height{0};

    constexpr Extent2D() = default;
    constexpr Extent2D(u32 w, u32 h) : width(w), height(h) {}
};

/**
 * @brief 3D extent (width, height, depth)
 */
struct Extent3D {
    u32 width{0};
    u32 height{0};
    u32 depth{1};

    constexpr Extent3D() = default;
    constexpr Extent3D(u32 w, u32 h, u32 d = 1) : width(w), height(h), depth(d) {}
    constexpr explicit Extent3D(const Extent2D& e2d)
        : width(e2d.width), height(e2d.height), depth(1) {}
};

/**
 * @brief 3D offset
 */
struct Offset3D {
    i32 x{0};
    i32 y{0};
    i32 z{0};

    constexpr Offset3D() = default;
    constexpr Offset3D(i32 x_, i32 y_, i32 z_ = 0) : x(x_), y(y_), z(z_) {}
};

// ============================================================================
// Device Limits
// ============================================================================

/**
 * @brief Hardware capability limits
 */
struct DeviceLimits {
    u32 max_texture_dimension_1d{16384};
    u32 max_texture_dimension_2d{16384};
    u32 max_texture_dimension_3d{2048};
    u32 max_texture_dimension_cube{16384};
    u32 max_texture_array_layers{2048};
    u32 max_uniform_buffer_size{65536};
    u32 max_storage_buffer_size{128 * 1024 * 1024};
    u32 max_push_constant_size{128};
    u32 max_bound_descriptor_sets{8};
    u32 max_vertex_input_attributes{32};
    u32 max_vertex_input_bindings{32};
    u32 max_vertex_input_attribute_offset{2047};
    u32 max_vertex_input_binding_stride{2048};
    u32 max_color_attachments{8};
    u32 max_compute_work_group_count[3]{65535, 65535, 65535};
    u32 max_compute_work_group_size[3]{1024, 1024, 64};
    u32 max_compute_work_group_invocations{1024};
    f32 max_sampler_anisotropy{16.0f};
    u32 min_uniform_buffer_offset_alignment{256};
    u32 min_storage_buffer_offset_alignment{256};
    u32 timestamp_period_ns{1}; // Nanoseconds per timestamp tick

    // Feature support
    bool supports_geometry_shader{true};
    bool supports_tessellation{true};
    bool supports_compute{true};
    bool supports_multi_draw_indirect{true};
    bool supports_bindless{false};
    bool supports_ray_tracing{false};
    bool supports_mesh_shaders{false};
    bool supports_variable_rate_shading{false};
};

// ============================================================================
// Device Info
// ============================================================================

/**
 * @brief GPU device type classification
 */
enum class DeviceType : u8 {
    Other,
    IntegratedGPU,
    DiscreteGPU,
    VirtualGPU,
    CPU,
};

/**
 * @brief GPU vendor identification
 */
enum class Vendor : u8 {
    Unknown,
    AMD,
    NVIDIA,
    Intel,
    ARM,
    Qualcomm,
    Apple,
    Microsoft, // WARP
};

/**
 * @brief Information about a physical GPU device
 */
struct DeviceInfo {
    char name[256]{};
    DeviceType type{DeviceType::Other};
    Vendor vendor{Vendor::Unknown};
    u32 vendor_id{0};
    u32 device_id{0};
    u32 driver_version{0};
    u64 dedicated_video_memory{0};
    u64 dedicated_system_memory{0};
    u64 shared_system_memory{0};
};

// ============================================================================
// RHI Handle Type (for backend-specific resource handles)
// ============================================================================

/**
 * @brief Type-safe handle for RHI resources
 *
 * Wraps a 64-bit value that can hold native API handles.
 * - Vulkan: VkBuffer, VkImage, etc. (64-bit handles)
 * - DX12: ID3D12Resource* (pointer)
 * - OpenGL: GLuint (fits in 64-bit)
 */
template <typename Tag>
struct RHIHandle {
    u64 value{0};

    constexpr RHIHandle() = default;
    constexpr explicit RHIHandle(u64 v) : value(v) {}

    [[nodiscard]] constexpr bool is_valid() const noexcept { return value != 0; }
    [[nodiscard]] constexpr explicit operator bool() const noexcept { return is_valid(); }
    [[nodiscard]] constexpr bool operator==(const RHIHandle& other) const noexcept = default;
};

// Concrete handle types
using BufferHandle = RHIHandle<struct BufferHandleTag>;
using TextureHandle = RHIHandle<struct TextureHandleTag>;
using SamplerHandle = RHIHandle<struct SamplerHandleTag>;
using PipelineHandle = RHIHandle<struct PipelineHandleTag>;
using ShaderHandle = RHIHandle<struct ShaderHandleTag>;
using DescriptorSetHandle = RHIHandle<struct DescriptorSetHandleTag>;
using FenceHandle = RHIHandle<struct FenceHandleTag>;
using SemaphoreHandle = RHIHandle<struct SemaphoreHandleTag>;

} // namespace hz::rhi
