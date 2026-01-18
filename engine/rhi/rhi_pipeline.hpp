#pragma once

/**
 * @file rhi_pipeline.hpp
 * @brief RHI Pipeline State Objects, Render Pass, and Pipeline Layout
 *
 * Defines the immutable pipeline state objects that encapsulate all rendering state.
 * This follows the Vulkan/DX12 model where pipeline state is baked at creation time.
 */

#include "rhi_resources.hpp"
#include "rhi_types.hpp"

#include <vector>

namespace hz::rhi {

// ============================================================================
// Vertex Input Configuration
// ============================================================================

/**
 * @brief Describes a vertex buffer binding point
 */
struct VertexBinding {
    u32 binding{0}; ///< Binding slot index
    u32 stride{0};  ///< Bytes between consecutive vertices
    VertexInputRate input_rate{VertexInputRate::Vertex};
};

/**
 * @brief Describes a vertex attribute within a binding
 */
struct VertexAttribute {
    u32 location{0}; ///< Shader input location
    u32 binding{0};  ///< Which VertexBinding this comes from
    Format format{Format::RGB32_FLOAT};
    u32 offset{0}; ///< Byte offset within the vertex
};

/**
 * @brief Complete vertex input layout description
 */
struct VertexInputLayout {
    std::vector<VertexBinding> bindings;
    std::vector<VertexAttribute> attributes;

    /**
     * @brief Create a simple layout with position, normal, UV, tangent
     * @return Layout matching the engine's standard Vertex struct
     */
    [[nodiscard]] static VertexInputLayout standard_vertex() {
        VertexInputLayout layout;

        // Single binding with standard vertex stride
        layout.bindings.push_back({.binding = 0,
                                   .stride = sizeof(f32) * (3 + 3 + 2 + 4) + sizeof(i32) * 4 +
                                             sizeof(f32) * 4, // ~80 bytes
                                   .input_rate = VertexInputRate::Vertex});

        u32 offset = 0;

        // Position (vec3)
        layout.attributes.push_back({0, 0, Format::RGB32_FLOAT, offset});
        offset += sizeof(f32) * 3;

        // Normal (vec3)
        layout.attributes.push_back({1, 0, Format::RGB32_FLOAT, offset});
        offset += sizeof(f32) * 3;

        // TexCoord (vec2)
        layout.attributes.push_back({2, 0, Format::RG32_FLOAT, offset});
        offset += sizeof(f32) * 2;

        // Tangent (vec4)
        layout.attributes.push_back({3, 0, Format::RGBA32_FLOAT, offset});
        offset += sizeof(f32) * 4;

        // Bone IDs (ivec4)
        layout.attributes.push_back({4, 0, Format::RGBA32_SINT, offset});
        offset += sizeof(i32) * 4;

        // Bone Weights (vec4)
        layout.attributes.push_back({5, 0, Format::RGBA32_FLOAT, offset});

        return layout;
    }

    /**
     * @brief Create a simple position-only layout
     */
    [[nodiscard]] static VertexInputLayout position_only() {
        VertexInputLayout layout;
        layout.bindings.push_back({0, sizeof(f32) * 3, VertexInputRate::Vertex});
        layout.attributes.push_back({0, 0, Format::RGB32_FLOAT, 0});
        return layout;
    }

    /**
     * @brief Create a 2D layout (position + UV)
     */
    [[nodiscard]] static VertexInputLayout position_uv() {
        VertexInputLayout layout;
        layout.bindings.push_back({0, sizeof(f32) * 4, VertexInputRate::Vertex});
        layout.attributes.push_back({0, 0, Format::RG32_FLOAT, 0});               // Position
        layout.attributes.push_back({1, 0, Format::RG32_FLOAT, sizeof(f32) * 2}); // UV
        return layout;
    }
};

// ============================================================================
// Rasterization State
// ============================================================================

/**
 * @brief Rasterization state configuration
 */
struct RasterizationState {
    PolygonMode polygon_mode{PolygonMode::Fill};
    CullMode cull_mode{CullMode::Back};
    FrontFace front_face{FrontFace::CounterClockwise};
    bool depth_clamp_enable{false};
    bool rasterizer_discard_enable{false};
    bool depth_bias_enable{false};
    f32 depth_bias_constant{0.0f};
    f32 depth_bias_clamp{0.0f};
    f32 depth_bias_slope{0.0f};
    f32 line_width{1.0f};
    bool conservative_rasterization{false};

    [[nodiscard]] static RasterizationState default_state() { return {}; }

    [[nodiscard]] static RasterizationState no_cull() {
        RasterizationState state;
        state.cull_mode = CullMode::None;
        return state;
    }

    [[nodiscard]] static RasterizationState front_cull() {
        RasterizationState state;
        state.cull_mode = CullMode::Front;
        return state;
    }

    [[nodiscard]] static RasterizationState wireframe() {
        RasterizationState state;
        state.polygon_mode = PolygonMode::Line;
        state.cull_mode = CullMode::None;
        return state;
    }

    [[nodiscard]] static RasterizationState shadow_map() {
        RasterizationState state;
        state.cull_mode = CullMode::Front; // Reduce peter-panning
        state.depth_bias_enable = true;
        state.depth_bias_constant = 1.25f;
        state.depth_bias_slope = 1.75f;
        return state;
    }
};

// ============================================================================
// Depth-Stencil State
// ============================================================================

/**
 * @brief Stencil operation state for one face
 */
struct StencilOpState {
    StencilOp fail_op{StencilOp::Keep};
    StencilOp pass_op{StencilOp::Keep};
    StencilOp depth_fail_op{StencilOp::Keep};
    CompareOp compare_op{CompareOp::Always};
    u32 compare_mask{0xFF};
    u32 write_mask{0xFF};
    u32 reference{0};
};

/**
 * @brief Depth-stencil state configuration
 */
struct DepthStencilState {
    bool depth_test_enable{true};
    bool depth_write_enable{true};
    CompareOp depth_compare_op{CompareOp::Less};
    bool depth_bounds_test_enable{false};
    f32 min_depth_bounds{0.0f};
    f32 max_depth_bounds{1.0f};
    bool stencil_test_enable{false};
    StencilOpState front;
    StencilOpState back;

    [[nodiscard]] static DepthStencilState default_state() { return {}; }

    [[nodiscard]] static DepthStencilState disabled() {
        DepthStencilState state;
        state.depth_test_enable = false;
        state.depth_write_enable = false;
        return state;
    }

    [[nodiscard]] static DepthStencilState read_only() {
        DepthStencilState state;
        state.depth_test_enable = true;
        state.depth_write_enable = false;
        return state;
    }

    [[nodiscard]] static DepthStencilState less_equal() {
        DepthStencilState state;
        state.depth_compare_op = CompareOp::LessOrEqual;
        return state;
    }

    [[nodiscard]] static DepthStencilState reverse_z() {
        DepthStencilState state;
        state.depth_compare_op = CompareOp::Greater;
        return state;
    }
};

// ============================================================================
// Blend State
// ============================================================================

/**
 * @brief Blend state for a single color attachment
 */
struct BlendAttachmentState {
    bool blend_enable{false};
    BlendFactor src_color_factor{BlendFactor::One};
    BlendFactor dst_color_factor{BlendFactor::Zero};
    BlendOp color_blend_op{BlendOp::Add};
    BlendFactor src_alpha_factor{BlendFactor::One};
    BlendFactor dst_alpha_factor{BlendFactor::Zero};
    BlendOp alpha_blend_op{BlendOp::Add};
    ColorWriteMask color_write_mask{ColorWriteMask::All};

    [[nodiscard]] static BlendAttachmentState disabled() { return {}; }

    [[nodiscard]] static BlendAttachmentState alpha_blend() {
        BlendAttachmentState state;
        state.blend_enable = true;
        state.src_color_factor = BlendFactor::SrcAlpha;
        state.dst_color_factor = BlendFactor::OneMinusSrcAlpha;
        state.color_blend_op = BlendOp::Add;
        state.src_alpha_factor = BlendFactor::One;
        state.dst_alpha_factor = BlendFactor::OneMinusSrcAlpha;
        state.alpha_blend_op = BlendOp::Add;
        return state;
    }

    [[nodiscard]] static BlendAttachmentState premultiplied_alpha() {
        BlendAttachmentState state;
        state.blend_enable = true;
        state.src_color_factor = BlendFactor::One;
        state.dst_color_factor = BlendFactor::OneMinusSrcAlpha;
        state.color_blend_op = BlendOp::Add;
        state.src_alpha_factor = BlendFactor::One;
        state.dst_alpha_factor = BlendFactor::OneMinusSrcAlpha;
        state.alpha_blend_op = BlendOp::Add;
        return state;
    }

    [[nodiscard]] static BlendAttachmentState additive() {
        BlendAttachmentState state;
        state.blend_enable = true;
        state.src_color_factor = BlendFactor::One;
        state.dst_color_factor = BlendFactor::One;
        state.color_blend_op = BlendOp::Add;
        state.src_alpha_factor = BlendFactor::One;
        state.dst_alpha_factor = BlendFactor::One;
        state.alpha_blend_op = BlendOp::Add;
        return state;
    }
};

/**
 * @brief Complete blend state for all attachments
 */
struct BlendState {
    std::vector<BlendAttachmentState> attachments;
    bool logic_op_enable{false};
    f32 blend_constants[4]{0.0f, 0.0f, 0.0f, 0.0f};

    [[nodiscard]] static BlendState disabled(u32 attachment_count = 1) {
        BlendState state;
        state.attachments.resize(attachment_count, BlendAttachmentState::disabled());
        return state;
    }

    [[nodiscard]] static BlendState alpha_blend(u32 attachment_count = 1) {
        BlendState state;
        state.attachments.resize(attachment_count, BlendAttachmentState::alpha_blend());
        return state;
    }
};

// ============================================================================
// Multisample State
// ============================================================================

/**
 * @brief Multisample anti-aliasing configuration
 */
struct MultisampleState {
    u32 sample_count{1};
    bool sample_shading_enable{false};
    f32 min_sample_shading{1.0f};
    bool alpha_to_coverage_enable{false};
    bool alpha_to_one_enable{false};
};

// ============================================================================
// Render Pass
// ============================================================================

/**
 * @brief Description of a single attachment in a render pass
 */
struct AttachmentDesc {
    Format format{Format::Unknown};
    u32 sample_count{1};
    LoadOp load_op{LoadOp::Clear};
    StoreOp store_op{StoreOp::Store};
    LoadOp stencil_load_op{LoadOp::DontCare};
    StoreOp stencil_store_op{StoreOp::DontCare};
    ResourceState initial_state{ResourceState::Undefined};
    ResourceState final_state{ResourceState::ShaderResource};
};

/**
 * @brief Description for creating a render pass
 */
struct RenderPassDesc {
    std::vector<AttachmentDesc> color_attachments;
    AttachmentDesc depth_stencil_attachment;
    bool has_depth_stencil{false};
    const char* debug_name{nullptr};

    /**
     * @brief Create a simple single-target render pass
     */
    [[nodiscard]] static RenderPassDesc simple(Format color_format,
                                               Format depth_format = Format::D32_FLOAT) {
        RenderPassDesc desc;

        desc.color_attachments.push_back({.format = color_format,
                                          .load_op = LoadOp::Clear,
                                          .store_op = StoreOp::Store,
                                          .initial_state = ResourceState::Undefined,
                                          .final_state = ResourceState::ShaderResource});

        if (depth_format != Format::Unknown) {
            desc.has_depth_stencil = true;
            desc.depth_stencil_attachment = {.format = depth_format,
                                             .load_op = LoadOp::Clear,
                                             .store_op = StoreOp::Store,
                                             .stencil_load_op = LoadOp::DontCare,
                                             .stencil_store_op = StoreOp::DontCare,
                                             .initial_state = ResourceState::Undefined,
                                             .final_state = ResourceState::DepthRead};
        }

        return desc;
    }

    /**
     * @brief Create a G-Buffer render pass with multiple color attachments
     */
    [[nodiscard]] static RenderPassDesc gbuffer() {
        RenderPassDesc desc;

        // Albedo + Metallic
        desc.color_attachments.push_back({Format::RGBA16_FLOAT, 1, LoadOp::Clear, StoreOp::Store});
        // Normal + Roughness
        desc.color_attachments.push_back({Format::RGBA16_FLOAT, 1, LoadOp::Clear, StoreOp::Store});
        // Emission + Material ID
        desc.color_attachments.push_back({Format::RGBA16_FLOAT, 1, LoadOp::Clear, StoreOp::Store});
        // Velocity
        desc.color_attachments.push_back({Format::RG16_FLOAT, 1, LoadOp::Clear, StoreOp::Store});

        desc.has_depth_stencil = true;
        desc.depth_stencil_attachment = {Format::D32_FLOAT, 1, LoadOp::Clear, StoreOp::Store};

        return desc;
    }

    /**
     * @brief Create a shadow map render pass (depth only)
     */
    [[nodiscard]] static RenderPassDesc shadow_map(Format depth_format = Format::D32_FLOAT) {
        RenderPassDesc desc;
        desc.has_depth_stencil = true;
        desc.depth_stencil_attachment = {.format = depth_format,
                                         .load_op = LoadOp::Clear,
                                         .store_op = StoreOp::Store,
                                         .initial_state = ResourceState::Undefined,
                                         .final_state = ResourceState::ShaderResource};
        return desc;
    }
};

/**
 * @brief Abstract render pass interface
 */
class RenderPass {
public:
    virtual ~RenderPass() = default;

    [[nodiscard]] virtual u32 color_attachment_count() const noexcept = 0;
    [[nodiscard]] virtual bool has_depth_stencil() const noexcept = 0;
    [[nodiscard]] virtual Format color_format(u32 index) const noexcept = 0;
    [[nodiscard]] virtual Format depth_stencil_format() const noexcept = 0;

    [[nodiscard]] virtual u64 native_handle() const noexcept = 0;

protected:
    RenderPass() = default;
    HZ_NON_COPYABLE(RenderPass);
    HZ_NON_MOVABLE(RenderPass);
};

// ============================================================================
// Framebuffer
// ============================================================================

/**
 * @brief Description for creating a framebuffer
 */
struct FramebufferDesc {
    const RenderPass* render_pass{nullptr};
    std::vector<const TextureView*> color_attachments;
    const TextureView* depth_stencil_attachment{nullptr};
    u32 width{0};
    u32 height{0};
    u32 layers{1};
    const char* debug_name{nullptr};
};

/**
 * @brief Abstract framebuffer interface
 */
class Framebuffer {
public:
    virtual ~Framebuffer() = default;

    [[nodiscard]] virtual u32 width() const noexcept = 0;
    [[nodiscard]] virtual u32 height() const noexcept = 0;
    [[nodiscard]] virtual u32 layers() const noexcept = 0;
    [[nodiscard]] virtual const RenderPass& render_pass() const noexcept = 0;

    [[nodiscard]] Extent2D extent() const noexcept { return {width(), height()}; }

    [[nodiscard]] virtual u64 native_handle() const noexcept = 0;

protected:
    Framebuffer() = default;
    HZ_NON_COPYABLE(Framebuffer);
    HZ_NON_MOVABLE(Framebuffer);
};

// ============================================================================
// Push Constant Range
// ============================================================================

/**
 * @brief Describes a range of push constant data
 */
struct PushConstantRange {
    ShaderStage stages{ShaderStage::None};
    u32 offset{0};
    u32 size{0};
};

// ============================================================================
// Pipeline Layout
// ============================================================================

// Forward declaration
class DescriptorSetLayout;

/**
 * @brief Description for creating a pipeline layout
 */
struct PipelineLayoutDesc {
    std::vector<const DescriptorSetLayout*> set_layouts;
    std::vector<PushConstantRange> push_constant_ranges;
    const char* debug_name{nullptr};
};

/**
 * @brief Abstract pipeline layout interface
 *
 * Defines the interface between shaders and resources (descriptor sets + push constants)
 */
class PipelineLayout {
public:
    virtual ~PipelineLayout() = default;

    [[nodiscard]] virtual u32 descriptor_set_count() const noexcept = 0;
    [[nodiscard]] virtual u32 push_constant_size() const noexcept = 0;

    [[nodiscard]] virtual u64 native_handle() const noexcept = 0;

protected:
    PipelineLayout() = default;
    HZ_NON_COPYABLE(PipelineLayout);
    HZ_NON_MOVABLE(PipelineLayout);
};

// ============================================================================
// Graphics Pipeline
// ============================================================================

/**
 * @brief Description for creating a graphics pipeline
 */
struct GraphicsPipelineDesc {
    // Shaders
    const ShaderModule* vertex_shader{nullptr};
    const ShaderModule* fragment_shader{nullptr};
    const ShaderModule* geometry_shader{nullptr};
    const ShaderModule* tess_control_shader{nullptr};
    const ShaderModule* tess_eval_shader{nullptr};

    // Vertex input
    VertexInputLayout vertex_layout;

    // Input assembly
    PrimitiveTopology topology{PrimitiveTopology::TriangleList};
    bool primitive_restart_enable{false};

    // Fixed function state
    RasterizationState rasterization;
    DepthStencilState depth_stencil;
    BlendState blend;
    MultisampleState multisample;

    // Dynamic state (can be changed without recreating pipeline)
    bool dynamic_viewport{true};
    bool dynamic_scissor{true};
    bool dynamic_line_width{false};
    bool dynamic_depth_bias{false};
    bool dynamic_blend_constants{false};
    bool dynamic_stencil_reference{false};

    // Layout and render pass
    const PipelineLayout* layout{nullptr};
    const RenderPass* render_pass{nullptr};
    u32 subpass{0};

    const char* debug_name{nullptr};
};

/**
 * @brief Description for creating a compute pipeline
 */
struct ComputePipelineDesc {
    const ShaderModule* compute_shader{nullptr};
    const PipelineLayout* layout{nullptr};
    const char* debug_name{nullptr};
};

/**
 * @brief Abstract pipeline interface
 *
 * Represents a compiled graphics or compute pipeline.
 */
class Pipeline {
public:
    virtual ~Pipeline() = default;

    /**
     * @brief Check if this is a compute pipeline
     */
    [[nodiscard]] virtual bool is_compute() const noexcept = 0;

    /**
     * @brief Get the pipeline layout
     */
    [[nodiscard]] virtual const PipelineLayout& layout() const noexcept = 0;

    [[nodiscard]] virtual u64 native_handle() const noexcept = 0;

protected:
    Pipeline() = default;
    HZ_NON_COPYABLE(Pipeline);
    HZ_NON_MOVABLE(Pipeline);
};

} // namespace hz::rhi
