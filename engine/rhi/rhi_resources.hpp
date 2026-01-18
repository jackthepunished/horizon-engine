#pragma once

/**
 * @file rhi_resources.hpp
 * @brief RHI resource interfaces: Buffer, Texture, Sampler, ShaderModule, Swapchain
 *
 * Defines abstract interfaces for GPU resources that are implemented by each backend.
 */

#include "rhi_types.hpp"

#include <span>
#include <string>

namespace hz::rhi {

// ============================================================================
// Buffer Descriptor
// ============================================================================

/**
 * @brief Description for creating a buffer
 */
struct BufferDesc {
    u64 size{0};                               ///< Size in bytes
    BufferUsage usage{BufferUsage::None};      ///< How the buffer will be used
    MemoryUsage memory{MemoryUsage::GPU_Only}; ///< Memory allocation strategy
    const void* initial_data{nullptr};         ///< Optional initial data to upload
    const char* debug_name{nullptr};           ///< Debug name for graphics debuggers
};

// ============================================================================
// Buffer Interface
// ============================================================================

/**
 * @brief Abstract GPU buffer interface
 *
 * Represents a linear memory allocation on the GPU. Can be used for vertices,
 * indices, uniforms, storage, etc. depending on usage flags.
 */
class Buffer {
public:
    virtual ~Buffer() = default;

    // ========================================================================
    // Properties
    // ========================================================================

    [[nodiscard]] virtual u64 size() const noexcept = 0;
    [[nodiscard]] virtual BufferUsage usage() const noexcept = 0;
    [[nodiscard]] virtual MemoryUsage memory_usage() const noexcept = 0;

    // ========================================================================
    // CPU Access (only valid for CPU-visible memory)
    // ========================================================================

    /**
     * @brief Map the buffer for CPU access
     * @return Pointer to mapped memory, or nullptr if mapping failed
     * @note Only valid for CPU_To_GPU, GPU_To_CPU, or CPU_Only memory
     */
    [[nodiscard]] virtual void* map() = 0;

    /**
     * @brief Unmap previously mapped memory
     */
    virtual void unmap() = 0;

    /**
     * @brief Flush mapped memory range to make writes visible to GPU
     * @param offset Byte offset into the buffer
     * @param size Number of bytes to flush (UINT64_MAX = remaining)
     * @note Only needed for non-coherent memory (rare)
     */
    virtual void flush(u64 offset = 0, u64 size = UINT64_MAX) = 0;

    /**
     * @brief Invalidate mapped memory range to see GPU writes
     * @param offset Byte offset into the buffer
     * @param size Number of bytes to invalidate (UINT64_MAX = remaining)
     * @note Only needed for readback buffers
     */
    virtual void invalidate(u64 offset = 0, u64 size = UINT64_MAX) = 0;

    // ========================================================================
    // Convenience Methods
    // ========================================================================

    /**
     * @brief Upload data to the buffer (for CPU-visible buffers)
     * @param data Pointer to source data
     * @param size Number of bytes to upload
     * @param offset Byte offset into the buffer
     */
    virtual void upload(const void* data, u64 size, u64 offset = 0) {
        void* mapped = map();
        if (mapped) {
            std::memcpy(static_cast<u8*>(mapped) + offset, data, size);
            flush(offset, size);
            unmap();
        }
    }

    /**
     * @brief Upload typed data to the buffer
     */
    template <typename T>
    void upload(std::span<const T> data, u64 offset = 0) {
        upload(data.data(), data.size_bytes(), offset);
    }

    /**
     * @brief Upload a single struct to the buffer
     */
    template <typename T>
    void upload(const T& data, u64 offset = 0) {
        upload(&data, sizeof(T), offset);
    }

    // ========================================================================
    // Native Handle
    // ========================================================================

    /**
     * @brief Get the backend-specific native handle
     * @return Vulkan: VkBuffer, DX12: ID3D12Resource*, OpenGL: GLuint
     */
    [[nodiscard]] virtual u64 native_handle() const noexcept = 0;

protected:
    Buffer() = default;
    HZ_NON_COPYABLE(Buffer);
    HZ_NON_MOVABLE(Buffer);
};

// ============================================================================
// Texture Descriptor
// ============================================================================

/**
 * @brief Description for creating a texture
 */
struct TextureDesc {
    TextureType type{TextureType::Texture2D};
    Format format{Format::RGBA8_UNORM};
    u32 width{1};
    u32 height{1};
    u32 depth{1};        ///< For 3D textures, otherwise 1
    u32 mip_levels{1};   ///< 0 = calculate full chain
    u32 array_layers{1}; ///< For array textures, 6 for cubemaps
    u32 sample_count{1}; ///< MSAA sample count
    TextureUsage usage{TextureUsage::Sampled};
    ResourceState initial_state{ResourceState::Undefined};
    ClearValue optimized_clear_value{}; ///< Optimized clear value hint
    const char* debug_name{nullptr};

    /**
     * @brief Calculate the number of mip levels for a full mip chain
     */
    [[nodiscard]] static constexpr u32 calculate_mip_levels(u32 width, u32 height, u32 depth = 1) {
        u32 max_dim = std::max({width, height, depth});
        u32 levels = 1;
        while (max_dim > 1) {
            max_dim >>= 1;
            ++levels;
        }
        return levels;
    }

    /**
     * @brief Create descriptor for a 2D texture
     */
    [[nodiscard]] static TextureDesc texture_2d(u32 width, u32 height, Format format,
                                                TextureUsage usage = TextureUsage::Sampled,
                                                u32 mip_levels = 1) {
        TextureDesc desc;
        desc.type = TextureType::Texture2D;
        desc.width = width;
        desc.height = height;
        desc.format = format;
        desc.usage = usage;
        desc.mip_levels = mip_levels;
        return desc;
    }

    /**
     * @brief Create descriptor for a render target
     */
    [[nodiscard]] static TextureDesc render_target(u32 width, u32 height, Format format,
                                                   u32 sample_count = 1) {
        TextureDesc desc;
        desc.type = TextureType::Texture2D;
        desc.width = width;
        desc.height = height;
        desc.format = format;
        desc.sample_count = sample_count;
        desc.mip_levels = 1;
        desc.usage = TextureUsage::RenderTarget | TextureUsage::Sampled;
        return desc;
    }

    /**
     * @brief Create descriptor for a depth-stencil texture
     */
    [[nodiscard]] static TextureDesc
    depth_stencil(u32 width, u32 height, Format format = Format::D32_FLOAT, u32 sample_count = 1) {
        TextureDesc desc;
        desc.type = TextureType::Texture2D;
        desc.width = width;
        desc.height = height;
        desc.format = format;
        desc.sample_count = sample_count;
        desc.mip_levels = 1;
        desc.usage = TextureUsage::DepthStencil | TextureUsage::Sampled;
        desc.optimized_clear_value = ClearDepthStencil{1.0f, 0};
        return desc;
    }

    /**
     * @brief Create descriptor for a cubemap
     */
    [[nodiscard]] static TextureDesc cubemap(u32 size, Format format, u32 mip_levels = 1) {
        TextureDesc desc;
        desc.type = TextureType::TextureCube;
        desc.width = size;
        desc.height = size;
        desc.array_layers = 6;
        desc.format = format;
        desc.mip_levels = mip_levels;
        desc.usage = TextureUsage::Sampled;
        return desc;
    }
};

// ============================================================================
// Texture Interface
// ============================================================================

/**
 * @brief Abstract GPU texture interface
 *
 * Represents an image resource on the GPU. Can be 1D, 2D, 3D, cube, or array.
 */
class Texture {
public:
    virtual ~Texture() = default;

    // ========================================================================
    // Properties
    // ========================================================================

    [[nodiscard]] virtual TextureType type() const noexcept = 0;
    [[nodiscard]] virtual Format format() const noexcept = 0;
    [[nodiscard]] virtual u32 width() const noexcept = 0;
    [[nodiscard]] virtual u32 height() const noexcept = 0;
    [[nodiscard]] virtual u32 depth() const noexcept = 0;
    [[nodiscard]] virtual u32 mip_levels() const noexcept = 0;
    [[nodiscard]] virtual u32 array_layers() const noexcept = 0;
    [[nodiscard]] virtual u32 sample_count() const noexcept = 0;
    [[nodiscard]] virtual TextureUsage usage() const noexcept = 0;

    /**
     * @brief Get extent at a specific mip level
     */
    [[nodiscard]] Extent3D mip_extent(u32 mip_level = 0) const noexcept {
        return {std::max(1u, width() >> mip_level), std::max(1u, height() >> mip_level),
                std::max(1u, depth() >> mip_level)};
    }

    // ========================================================================
    // Native Handle
    // ========================================================================

    [[nodiscard]] virtual u64 native_handle() const noexcept = 0;

protected:
    Texture() = default;
    HZ_NON_COPYABLE(Texture);
    HZ_NON_MOVABLE(Texture);
};

// ============================================================================
// Texture View Descriptor
// ============================================================================

/**
 * @brief Description for creating a texture view
 *
 * A view into a texture that can provide a different interpretation
 * (e.g., view a single mip level, or a single array layer).
 */
struct TextureViewDesc {
    const Texture* texture{nullptr};
    TextureType view_type{TextureType::Texture2D}; ///< Can differ from texture type
    Format format{Format::Unknown};                ///< Unknown = inherit from texture
    u32 base_mip_level{0};
    u32 mip_level_count{1}; ///< UINT32_MAX = remaining mips
    u32 base_array_layer{0};
    u32 array_layer_count{1}; ///< UINT32_MAX = remaining layers
    const char* debug_name{nullptr};
};

// ============================================================================
// Texture View Interface
// ============================================================================

/**
 * @brief Abstract texture view interface
 *
 * Provides a specific view into a texture resource.
 */
class TextureView {
public:
    virtual ~TextureView() = default;

    [[nodiscard]] virtual const Texture& texture() const noexcept = 0;
    [[nodiscard]] virtual TextureType view_type() const noexcept = 0;
    [[nodiscard]] virtual Format format() const noexcept = 0;
    [[nodiscard]] virtual u32 base_mip_level() const noexcept = 0;
    [[nodiscard]] virtual u32 mip_level_count() const noexcept = 0;
    [[nodiscard]] virtual u32 base_array_layer() const noexcept = 0;
    [[nodiscard]] virtual u32 array_layer_count() const noexcept = 0;

    [[nodiscard]] virtual u64 native_handle() const noexcept = 0;

protected:
    TextureView() = default;
    HZ_NON_COPYABLE(TextureView);
    HZ_NON_MOVABLE(TextureView);
};

// ============================================================================
// Sampler Descriptor
// ============================================================================

/**
 * @brief Description for creating a sampler
 */
struct SamplerDesc {
    Filter min_filter{Filter::Linear};
    Filter mag_filter{Filter::Linear};
    MipmapMode mipmap_mode{MipmapMode::Linear};
    AddressMode address_u{AddressMode::Repeat};
    AddressMode address_v{AddressMode::Repeat};
    AddressMode address_w{AddressMode::Repeat};
    f32 mip_lod_bias{0.0f};
    bool anisotropy_enable{false};
    f32 max_anisotropy{1.0f};
    bool compare_enable{false};
    CompareOp compare_op{CompareOp::Never};
    f32 min_lod{0.0f};
    f32 max_lod{1000.0f};
    BorderColor border_color{BorderColor::OpaqueBlack};
    const char* debug_name{nullptr};

    /**
     * @brief Create a linear sampler with repeat addressing
     */
    [[nodiscard]] static SamplerDesc linear_repeat() {
        SamplerDesc desc;
        desc.min_filter = Filter::Linear;
        desc.mag_filter = Filter::Linear;
        desc.mipmap_mode = MipmapMode::Linear;
        return desc;
    }

    /**
     * @brief Create a point/nearest sampler
     */
    [[nodiscard]] static SamplerDesc point() {
        SamplerDesc desc;
        desc.min_filter = Filter::Nearest;
        desc.mag_filter = Filter::Nearest;
        desc.mipmap_mode = MipmapMode::Nearest;
        return desc;
    }

    /**
     * @brief Create a linear sampler with anisotropic filtering
     */
    [[nodiscard]] static SamplerDesc anisotropic(f32 max_aniso = 16.0f) {
        SamplerDesc desc;
        desc.min_filter = Filter::Linear;
        desc.mag_filter = Filter::Linear;
        desc.mipmap_mode = MipmapMode::Linear;
        desc.anisotropy_enable = true;
        desc.max_anisotropy = max_aniso;
        return desc;
    }

    /**
     * @brief Create a shadow map comparison sampler
     */
    [[nodiscard]] static SamplerDesc shadow() {
        SamplerDesc desc;
        desc.min_filter = Filter::Linear;
        desc.mag_filter = Filter::Linear;
        desc.mipmap_mode = MipmapMode::Nearest;
        desc.address_u = AddressMode::ClampToBorder;
        desc.address_v = AddressMode::ClampToBorder;
        desc.address_w = AddressMode::ClampToBorder;
        desc.border_color = BorderColor::OpaqueWhite;
        desc.compare_enable = true;
        desc.compare_op = CompareOp::LessOrEqual;
        return desc;
    }
};

// ============================================================================
// Sampler Interface
// ============================================================================

/**
 * @brief Abstract sampler interface
 *
 * Defines how textures are sampled (filtering, addressing, etc.)
 */
class Sampler {
public:
    virtual ~Sampler() = default;

    [[nodiscard]] virtual u64 native_handle() const noexcept = 0;

protected:
    Sampler() = default;
    HZ_NON_COPYABLE(Sampler);
    HZ_NON_MOVABLE(Sampler);
};

// ============================================================================
// Shader Module Descriptor
// ============================================================================

/**
 * @brief Description for creating a shader module
 */
struct ShaderModuleDesc {
    std::span<const u8> bytecode; ///< SPIR-V for Vulkan, DXIL for DX12, GLSL source for OpenGL
    ShaderStage stage{ShaderStage::None};
    const char* entry_point{"main"};
    const char* debug_name{nullptr};
};

// ============================================================================
// Shader Module Interface
// ============================================================================

/**
 * @brief Abstract compiled shader module
 */
class ShaderModule {
public:
    virtual ~ShaderModule() = default;

    [[nodiscard]] virtual ShaderStage stage() const noexcept = 0;
    [[nodiscard]] virtual const char* entry_point() const noexcept = 0;

    [[nodiscard]] virtual u64 native_handle() const noexcept = 0;

protected:
    ShaderModule() = default;
    HZ_NON_COPYABLE(ShaderModule);
    HZ_NON_MOVABLE(ShaderModule);
};

// ============================================================================
// Swapchain Descriptor
// ============================================================================

/**
 * @brief Description for creating a swapchain
 */
struct SwapchainDesc {
    void* window_handle{nullptr}; ///< Platform window handle (GLFWwindow*, HWND, etc.)
    u32 width{0};
    u32 height{0};
    Format format{Format::BGRA8_SRGB}; ///< Preferred format
    u32 buffer_count{3};               ///< Triple buffering recommended
    bool vsync{true};
    const char* debug_name{nullptr};
};

// ============================================================================
// Swapchain Interface
// ============================================================================

/**
 * @brief Abstract swapchain interface
 *
 * Manages the presentation of rendered frames to a window.
 */
class Swapchain {
public:
    virtual ~Swapchain() = default;

    // ========================================================================
    // Properties
    // ========================================================================

    [[nodiscard]] virtual u32 width() const noexcept = 0;
    [[nodiscard]] virtual u32 height() const noexcept = 0;
    [[nodiscard]] virtual Format format() const noexcept = 0;
    [[nodiscard]] virtual u32 image_count() const noexcept = 0;
    [[nodiscard]] virtual u32 current_image_index() const noexcept = 0;

    [[nodiscard]] Extent2D extent() const noexcept { return {width(), height()}; }

    // ========================================================================
    // Frame Operations
    // ========================================================================

    /**
     * @brief Get the texture for the current backbuffer
     * @return Texture that can be used as a render target
     */
    [[nodiscard]] virtual Texture* get_current_texture() = 0;

    /**
     * @brief Get the texture view for the current backbuffer
     * @return View suitable for rendering
     */
    [[nodiscard]] virtual TextureView* get_current_view() = 0;

    /**
     * @brief Acquire the next image for rendering
     * @param signal_semaphore Optional semaphore to signal when image is ready
     * @return true if successful, false if resize is needed
     */
    [[nodiscard]] virtual bool acquire_next_image(Semaphore* signal_semaphore = nullptr) = 0;

    /**
     * @brief Present the current image to the screen
     * @param wait_semaphores Semaphores to wait on before presenting
     */
    virtual void present(std::span<Semaphore* const> wait_semaphores = {}) = 0;

    /**
     * @brief Handle window resize
     * @param width New width
     * @param height New height
     */
    virtual void resize(u32 width, u32 height) = 0;

protected:
    Swapchain() = default;
    HZ_NON_COPYABLE(Swapchain);
    HZ_NON_MOVABLE(Swapchain);
};

// ============================================================================
// Synchronization Primitives
// ============================================================================

/**
 * @brief CPU-GPU synchronization fence
 *
 * Used to wait for GPU work to complete on the CPU side.
 */
class Fence {
public:
    virtual ~Fence() = default;

    /**
     * @brief Check if the fence has been signaled
     */
    [[nodiscard]] virtual bool is_signaled() const = 0;

    /**
     * @brief Wait for the fence to be signaled
     * @param timeout_ns Timeout in nanoseconds (UINT64_MAX = infinite)
     * @return true if signaled, false if timeout
     */
    [[nodiscard]] virtual bool wait(u64 timeout_ns = UINT64_MAX) = 0;

    /**
     * @brief Reset the fence to unsignaled state
     */
    virtual void reset() = 0;

    [[nodiscard]] virtual u64 native_handle() const noexcept = 0;

protected:
    Fence() = default;
    HZ_NON_COPYABLE(Fence);
    HZ_NON_MOVABLE(Fence);
};

/**
 * @brief GPU-GPU synchronization semaphore
 *
 * Used to synchronize work between command queues on the GPU.
 */
class Semaphore {
public:
    virtual ~Semaphore() = default;

    [[nodiscard]] virtual u64 native_handle() const noexcept = 0;

protected:
    Semaphore() = default;
    HZ_NON_COPYABLE(Semaphore);
    HZ_NON_MOVABLE(Semaphore);
};

} // namespace hz::rhi
