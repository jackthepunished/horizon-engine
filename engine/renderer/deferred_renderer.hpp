#pragma once

/**
 * @file deferred_renderer.hpp
 * @brief Industry-standard Deferred Rendering Pipeline
 *
 * A high-performance deferred renderer optimized for complex FPS scenes.
 *
 * Pipeline stages:
 * 1. Geometry Pass - Render scene to G-Buffer (MRT)
 * 2. SSAO Pass - Screen-space ambient occlusion
 * 3. Shadow Pass - Cascaded shadow maps
 * 4. Lighting Pass - Deferred lighting with all light types
 * 5. SSR Pass - Screen-space reflections
 * 6. Post-Process Pass - Bloom, TAA, Tone mapping
 *
 * G-Buffer Layout (optimized for bandwidth):
 * - RT0: RGB=Albedo, A=Metallic
 * - RT1: RG=Normal (octahedron encoded), B=Roughness, A=AO
 * - RT2: RGB=Emission, A=Material ID
 * - Depth: 32-bit float depth buffer
 */

#include "engine/core/types.hpp"
#include "engine/renderer/camera.hpp"
#include "engine/rhi/rhi_command_list.hpp"
#include "engine/rhi/rhi_device.hpp"
#include "engine/rhi/rhi_pipeline.hpp"
#include "engine/rhi/rhi_resources.hpp"

#include <array>
#include <memory>
#include <vector>

#include <glm/glm.hpp>

namespace hz {

// ============================================================================
// G-Buffer Configuration
// ============================================================================

/**
 * @brief G-Buffer render target indices
 */
enum GBufferTarget : u32 {
    GBUFFER_ALBEDO_METALLIC = 0,  // RGBA16F: RGB=Albedo, A=Metallic
    GBUFFER_NORMAL_ROUGHNESS = 1, // RGBA16F: RG=Normal, B=Roughness, A=AO
    GBUFFER_EMISSION_ID = 2,      // RGBA16F: RGB=Emission, A=Material ID
    GBUFFER_VELOCITY = 3,         // RG16F: RG=Velocity
    // Depth is separate
    GBUFFER_COUNT = 4
};

/**
 * @brief G-Buffer framebuffer with MRT support
 */
struct GBuffer {
    std::unique_ptr<rhi::Framebuffer> fbo;

    // Attachments
    std::array<std::unique_ptr<rhi::Texture>, GBUFFER_COUNT> colors;
    std::array<std::unique_ptr<rhi::TextureView>, GBUFFER_COUNT> color_views;

    std::unique_ptr<rhi::Texture> depth;
    std::unique_ptr<rhi::TextureView> depth_view;

    u32 width{0};
    u32 height{0};

    void create(rhi::Device& device, u32 w, u32 h);
    void destroy();

    [[nodiscard]] rhi::TextureView* get_view(u32 index) const { return color_views[index].get(); }
    [[nodiscard]] rhi::TextureView* get_depth_view() const { return depth_view.get(); }
};

// ============================================================================
// Cascaded Shadow Maps
// ============================================================================

/**
 * @brief Single cascade in the shadow map
 */
struct ShadowCascade {
    glm::mat4 view_projection;
    f32 split_depth;
    rhi::TextureView* view{nullptr}; // View into the array layer
};

/**
 * @brief Cascaded Shadow Map configuration
 */
struct CascadedShadowConfig {
    static constexpr u32 MAX_CASCADES = 4;

    u32 cascade_count{4};
    u32 resolution{2048};
    f32 split_lambda{0.75f};
    f32 shadow_distance{100.0f};
    f32 cascade_blend_distance{5.0f};

    u32 pcf_samples{16};
    f32 pcf_radius{2.0f};
    bool use_poisson_disk{true};
};

/**
 * @brief Cascaded Shadow Map system
 */
struct CascadedShadowMap {
    std::unique_ptr<rhi::Framebuffer> fbo; // Layered FBO? Or separate FBOs per cascade?
    std::unique_ptr<rhi::Texture> depth_array_texture;
    std::unique_ptr<rhi::TextureView> depth_array_view;

    std::array<ShadowCascade, CascadedShadowConfig::MAX_CASCADES> cascades;
    std::array<std::unique_ptr<rhi::TextureView>, CascadedShadowConfig::MAX_CASCADES> cascade_views;
    std::array<std::unique_ptr<rhi::Framebuffer>, CascadedShadowConfig::MAX_CASCADES>
        cascade_fbos; // One per layer

    CascadedShadowConfig config;

    void create(rhi::Device& device, const CascadedShadowConfig& cfg);
    void destroy();
    void update_cascades(const Camera& camera, const glm::vec3& light_dir);

private:
    void calculate_cascade_splits(const Camera& camera);
    [[nodiscard]] glm::mat4 calculate_light_space_matrix(u32 cascade, const Camera& camera,
                                                         const glm::vec3& light_dir);
};

// ============================================================================
// Screen Space Reflections
// ============================================================================

struct SSRConfig {
    f32 max_distance{50.0f};
    f32 resolution_scale{0.5f};
    u32 max_steps{64};
    u32 binary_search_steps{8};
    f32 thickness{0.5f};
    f32 stride{1.0f};
    f32 fade_start{0.8f};
    f32 fade_end{1.0f};
    bool enabled{true};
};

struct SSRPass {
    std::unique_ptr<rhi::Framebuffer> fbo;
    std::unique_ptr<rhi::Texture> color_texture;
    std::unique_ptr<rhi::TextureView> color_view;

    u32 width{0};
    u32 height{0};
    SSRConfig config;

    void create(rhi::Device& device, u32 w, u32 h, const SSRConfig& cfg);
    void destroy();
};

// ============================================================================
// Temporal Anti-Aliasing
// ============================================================================

struct TAAConfig {
    f32 feedback_min{0.75f};
    f32 feedback_max{0.90f};
    f32 jitter_scale{1.0f};
    bool enabled{false};
    bool use_motion_vectors{false};
};

struct TAAPass {
    std::unique_ptr<rhi::Framebuffer> fbo;

    std::unique_ptr<rhi::Texture> current_texture;
    std::unique_ptr<rhi::TextureView> current_view;

    std::unique_ptr<rhi::Texture> history_texture;
    std::unique_ptr<rhi::TextureView> history_view;

    std::unique_ptr<rhi::Texture> velocity_texture; // Derived from GBuffer usually
    std::unique_ptr<rhi::TextureView> velocity_view;

    u32 width{0};
    u32 height{0};
    u32 frame_index{0};
    TAAConfig config;

    static constexpr u32 JITTER_SAMPLE_COUNT = 16;
    std::array<glm::vec2, JITTER_SAMPLE_COUNT> jitter_offsets;

    void create(rhi::Device& device, u32 w, u32 h, const TAAConfig& cfg);
    void destroy();
    void swap_history();

    [[nodiscard]] glm::vec2 get_current_jitter() const;
    [[nodiscard]] glm::mat4 get_jittered_projection(const glm::mat4& proj) const;

private:
    void generate_halton_sequence();
};

// ============================================================================
// Light Volumes
// ============================================================================

struct GPUPointLight {
    glm::vec4 position_radius; // xyz = position, w = radius
    glm::vec4 color_intensity; // xyz = color, w = intensity
};

struct GPUSpotLight {
    glm::vec4 position_radius;     // xyz = position, w = radius
    glm::vec4 direction_cutoff;    // xyz = direction, w = cutoff angle
    glm::vec4 color_intensity;     // xyz = color, w = intensity
    glm::vec4 outer_cutoff_unused; // x = outer cutoff, yzw = unused
};

struct LightTile {
    u32 point_light_count{0};
    u32 spot_light_count{0};
    std::array<u16, 64> point_light_indices;
    std::array<u16, 32> spot_light_indices;
};

// ============================================================================
// Deferred Renderer
// ============================================================================

struct RenderStats {
    u32 draw_calls{0};
    u32 triangles{0};
    u32 visible_objects{0};
    u32 culled_objects{0};
    u32 active_lights{0};
    f32 geometry_pass_ms{0.0f};
    f32 lighting_pass_ms{0.0f};
    f32 shadow_pass_ms{0.0f};
    f32 post_process_ms{0.0f};
    f32 total_frame_ms{0.0f};
};

/**
 * @brief Full deferred rendering pipeline
 */
class DeferredRenderer {
public:
    DeferredRenderer(rhi::Device& device, rhi::Swapchain& swapchain);
    ~DeferredRenderer();

    HZ_NON_COPYABLE(DeferredRenderer);
    HZ_NON_MOVABLE(DeferredRenderer);

    /**
     * @brief Initialize the deferred renderer
     */
    [[nodiscard]] bool init();

    /**
     * @brief Shutdown and cleanup
     */
    void shutdown();

    /**
     * @brief Resize all render targets
     */
    void resize(u32 width, u32 height);

    // =========================================================================
    // Pipeline Stages
    // =========================================================================

    // Command lists are passed in or managed internally.
    // Prefer passing in cmd list for composability.

    void begin_geometry_pass(rhi::CommandList& cmd, const Camera& camera);
    void end_geometry_pass(rhi::CommandList& cmd);

    void render_shadows(rhi::CommandList& cmd, const glm::vec3& light_direction);

    void execute_lighting_pass(rhi::CommandList& cmd, const Camera& camera,
                               const std::vector<GPUPointLight>& point_lights,
                               const std::vector<GPUSpotLight>& spot_lights,
                               const glm::vec3& sun_direction, const glm::vec3& sun_color,
                               rhi::TextureView* irradiance_map = nullptr,
                               rhi::TextureView* prefilter_map = nullptr,
                               rhi::TextureView* brdf_lut = nullptr,
                               rhi::TextureView* environment_map = nullptr);

    void execute_ssr_pass(rhi::CommandList& cmd, const Camera& camera);

    void execute_taa_pass(rhi::CommandList& cmd);

    [[nodiscard]] glm::mat4 get_taa_jittered_projection(const glm::mat4& proj) const {
        return m_taa.config.enabled ? m_taa.get_jittered_projection(proj) : proj;
    }

    void execute_post_process(rhi::CommandList& cmd, const Camera& camera, f32 exposure,
                              f32 bloom_threshold, f32 bloom_intensity);

    void render_to_screen(rhi::CommandList& cmd);

    // =========================================================================
    // Configuration
    // =========================================================================

    void set_csm_config(const CascadedShadowConfig& config);
    void set_ssr_config(const SSRConfig& config);
    void set_taa_config(const TAAConfig& config);

    [[nodiscard]] const CascadedShadowConfig& get_csm_config() const { return m_csm.config; }
    [[nodiscard]] const SSRConfig& get_ssr_config() const { return m_ssr.config; }
    [[nodiscard]] const TAAConfig& get_taa_config() const { return m_taa.config; }

    // =========================================================================
    // Debug & Profiling
    // =========================================================================

    [[nodiscard]] const RenderStats& get_stats() const { return m_stats; }
    void reset_stats();

    [[nodiscard]] u32 get_final_output() const;

private:
    // Debug accessors (returning Views)
    [[nodiscard]] rhi::TextureView* get_gbuffer_albedo() const {
        return m_gbuffer.get_view(GBUFFER_ALBEDO_METALLIC);
    }
    // ... other accessors implemented in cpp or using helper ...

    /**
     * @brief Update frustum planes from camera
     */
    void update_frustum(const Camera& camera);

    /**
     * @brief Test if AABB is visible
     */
    [[nodiscard]] bool is_visible(const glm::vec3& min, const glm::vec3& max) const;

private:
    void create_pipelines();
    void create_fullscreen_quad();
    void render_fullscreen_quad(rhi::CommandList& cmd) const;
    void update_gbuffer_descriptor_set();

    rhi::Device& m_device;
    rhi::Swapchain& m_swapchain;

    // Dimensions
    u32 m_width{0};
    u32 m_height{0};

    // Pipeline stages
    GBuffer m_gbuffer;
    CascadedShadowMap m_csm;
    SSRPass m_ssr;
    TAAPass m_taa;

    // HDR + Post-process FBOs
    std::unique_ptr<rhi::Framebuffer> m_lighting_fbo;
    std::unique_ptr<rhi::Texture> m_lighting_texture;
    std::unique_ptr<rhi::TextureView> m_lighting_view;

    std::unique_ptr<rhi::Framebuffer> m_bloom_fbo;
    std::unique_ptr<rhi::Texture> m_bloom_texture;
    std::unique_ptr<rhi::TextureView> m_bloom_view;

    std::array<std::unique_ptr<rhi::Framebuffer>, 2> m_blur_fbos;
    std::array<std::unique_ptr<rhi::Texture>, 2> m_blur_textures;
    std::array<std::unique_ptr<rhi::TextureView>, 2> m_blur_views;

    // We don't need a final FBO if implementing presentation in swapchain,
    // but usually needed for post-process chain before composition.

    // Pipelines (instead of Shaders)
    // We will use PipelineLayout and Pipeline
    // For now, storing them as raw pointers or unique_ptrs to Pipeline
    // (Assuming Pipeline is RAII or managed by Device)

    // TODO: Define Pipeline structs or use generic Pipeline*
    // For this refactor, let's assume we store Pipelines.
    // Since Pipeline creation is complex, we might obscure it for now.

    // Fullscreen quad
    std::unique_ptr<rhi::Buffer> m_quad_vb;

    // Pipelines
    std::unique_ptr<rhi::Pipeline> m_geometry_pipeline;
    std::unique_ptr<rhi::PipelineLayout> m_geometry_layout;
    std::unique_ptr<rhi::Pipeline> m_lighting_pipeline;
    std::unique_ptr<rhi::PipelineLayout> m_lighting_layout;
    std::unique_ptr<rhi::Pipeline> m_composite_pipeline;
    std::unique_ptr<rhi::PipelineLayout> m_composite_layout;

    // Descriptor set layouts
    std::unique_ptr<rhi::DescriptorSetLayout> m_camera_layout;
    std::unique_ptr<rhi::DescriptorSetLayout> m_material_layout;
    std::unique_ptr<rhi::DescriptorSetLayout> m_gbuffer_input_layout;
    std::unique_ptr<rhi::DescriptorSetLayout> m_lighting_data_layout;
    std::unique_ptr<rhi::DescriptorSetLayout> m_composite_input_layout;

    // Descriptor pool and sets
    std::unique_ptr<rhi::DescriptorPool> m_descriptor_pool;
    std::unique_ptr<rhi::DescriptorSet> m_camera_set;
    std::unique_ptr<rhi::DescriptorSet> m_gbuffer_input_set;
    std::unique_ptr<rhi::DescriptorSet> m_lighting_data_set;
    std::unique_ptr<rhi::DescriptorSet> m_composite_input_set;

    // UBO buffers
    std::unique_ptr<rhi::Buffer> m_camera_ubo;
    std::unique_ptr<rhi::Buffer> m_light_ubo;
    std::unique_ptr<rhi::Buffer> m_point_light_ssbo;

    // Samplers
    std::unique_ptr<rhi::Sampler> m_linear_sampler;
    std::unique_ptr<rhi::Sampler> m_nearest_sampler;

    // Frustum planes
    std::array<glm::vec4, 6> m_frustum_planes;

    // Shadow state
    glm::mat4 m_light_space_matrix{1.0f};

    // Stats
    RenderStats m_stats;

    bool m_initialized{false};
};

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * @brief Encode normal to octahedron (2 components)
 */
[[nodiscard]] inline glm::vec2 encode_octahedron(const glm::vec3& n) {
    glm::vec3 nabs = glm::abs(n);
    glm::vec2 result = glm::vec2(n.x, n.y) / (nabs.x + nabs.y + nabs.z);
    if (n.z < 0.0f) {
        result = glm::vec2((1.0f - std::abs(result.y)) * (result.x >= 0.0f ? 1.0f : -1.0f),
                           (1.0f - std::abs(result.x)) * (result.y >= 0.0f ? 1.0f : -1.0f));
    }
    return result * 0.5f + 0.5f;
}

/**
 * @brief Decode octahedron to normal (from 2 components)
 */
[[nodiscard]] inline glm::vec3 decode_octahedron(const glm::vec2& f) {
    glm::vec2 f2 = f * 2.0f - 1.0f;
    glm::vec3 n(f2.x, f2.y, 1.0f - std::abs(f2.x) - std::abs(f2.y));
    f32 t = glm::clamp(-n.z, 0.0f, 1.0f);
    n.x += n.x >= 0.0f ? -t : t;
    n.y += n.y >= 0.0f ? -t : t;
    return glm::normalize(n);
}

} // namespace hz
