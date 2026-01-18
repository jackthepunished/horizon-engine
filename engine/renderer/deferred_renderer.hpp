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
#include "engine/renderer/opengl/framebuffer.hpp"
#include "engine/renderer/opengl/shader.hpp"

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
    GBUFFER_DEPTH_COPY = 4,       // R32F: Explicit depth copy to avoid sampler issues
    GBUFFER_COUNT = 5
};

/**
 * @brief G-Buffer framebuffer with MRT support
 */
struct GBuffer {
    u32 fbo{0};
    std::array<u32, GBUFFER_COUNT> color_textures{};
    u32 depth_texture{0};
    u32 width{0};
    u32 height{0};

    void create(u32 w, u32 h);
    void destroy();
    void bind() const;
    void unbind() const;
    void bind_textures(u32 start_slot = 0) const;
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
};

/**
 * @brief Cascaded Shadow Map configuration
 */
struct CascadedShadowConfig {
    static constexpr u32 MAX_CASCADES = 4;

    u32 cascade_count{4};
    u32 resolution{2048};
    f32 split_lambda{0.75f}; // Logarithmic vs linear split
    f32 shadow_distance{100.0f};
    f32 cascade_blend_distance{5.0f};

    // PCF settings
    u32 pcf_samples{16};
    f32 pcf_radius{2.0f};
    bool use_poisson_disk{true};
};

/**
 * @brief Cascaded Shadow Map system
 */
struct CascadedShadowMap {
    u32 fbo{0};
    u32 depth_array_texture{0}; // Texture array for all cascades
    std::array<ShadowCascade, CascadedShadowConfig::MAX_CASCADES> cascades;
    CascadedShadowConfig config;

    void create(const CascadedShadowConfig& cfg);
    void destroy();
    void bind_cascade(u32 cascade_index) const;
    void unbind() const;
    void update_cascades(const Camera& camera, const glm::vec3& light_dir);

private:
    void calculate_cascade_splits(const Camera& camera);
    glm::mat4 calculate_light_space_matrix(u32 cascade, const Camera& camera,
                                           const glm::vec3& light_dir);
};

// ============================================================================
// Screen Space Reflections
// ============================================================================

/**
 * @brief SSR configuration
 */
struct SSRConfig {
    f32 max_distance{50.0f};
    f32 resolution_scale{0.5f}; // Render at half resolution
    u32 max_steps{64};
    u32 binary_search_steps{8};
    f32 thickness{0.5f};
    f32 stride{1.0f};
    f32 fade_start{0.8f};
    f32 fade_end{1.0f};
    bool enabled{true};
};

/**
 * @brief SSR pass data
 */
struct SSRPass {
    u32 fbo{0};
    u32 color_texture{0};
    u32 width{0};
    u32 height{0};
    SSRConfig config;

    void create(u32 w, u32 h, const SSRConfig& cfg);
    void destroy();
    void bind() const;
    void unbind() const;
};

// ============================================================================
// Temporal Anti-Aliasing
// ============================================================================

/**
 * @brief TAA configuration
 */
struct TAAConfig {
    f32 feedback_min{0.75f};        // Lower = less blur, more responsive
    f32 feedback_max{0.90f};        // Reduced from 0.97 to reduce motion blur
    f32 jitter_scale{1.0f};         // Jitter intensity
    bool enabled{false};            // Disabled by default until TAA pass is properly called
    bool use_motion_vectors{false}; // Not implemented yet
};

/**
 * @brief TAA pass with history buffer
 */
struct TAAPass {
    u32 fbo{0};
    u32 current_texture{0};
    u32 history_texture{0};
    u32 velocity_texture{0}; // Motion vectors
    u32 width{0};
    u32 height{0};
    u32 frame_index{0};
    TAAConfig config;

    // Jitter offsets for subpixel sampling (Halton 2,3 sequence)
    static constexpr u32 JITTER_SAMPLE_COUNT = 16;
    std::array<glm::vec2, JITTER_SAMPLE_COUNT> jitter_offsets;

    void create(u32 w, u32 h, const TAAConfig& cfg);
    void destroy();
    void bind() const;
    void unbind() const;
    void swap_history();
    [[nodiscard]] glm::vec2 get_current_jitter() const;
    [[nodiscard]] glm::mat4 get_jittered_projection(const glm::mat4& proj) const;

private:
    void generate_halton_sequence();
};

// ============================================================================
// Light Volumes
// ============================================================================

/**
 * @brief GPU light data for deferred lighting
 */
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

/**
 * @brief Light culling tile for clustered deferred
 */
struct LightTile {
    u32 point_light_count{0};
    u32 spot_light_count{0};
    std::array<u16, 64> point_light_indices;
    std::array<u16, 32> spot_light_indices;
};

// ============================================================================
// Deferred Renderer
// ============================================================================

/**
 * @brief Render statistics for profiling
 */
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
    DeferredRenderer() = default;
    ~DeferredRenderer();

    HZ_NON_COPYABLE(DeferredRenderer);
    HZ_NON_MOVABLE(DeferredRenderer);

    /**
     * @brief Initialize the deferred renderer
     */
    bool init(u32 width, u32 height);

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

    /**
     * @brief Begin geometry pass (renders to G-Buffer)
     * @param camera Current camera
     */
    void begin_geometry_pass(const Camera& camera);

    /**
     * @brief End geometry pass
     */
    void end_geometry_pass();

    /**
     * @brief Render shadow maps
     * @param light_direction Direction to the sun
     */
    void render_shadows(const glm::vec3& light_direction);

    /**
     * @brief Begin shadow map pass (bind shadow FBO)
     * Caller should render depth-only geometry with a shadow shader.
     */
    void begin_shadow_pass(const glm::mat4& light_space_matrix);

    /**
     * @brief End shadow map pass (unbind shadow FBO and restore viewport)
     */
    void end_shadow_pass();

    /**
     * @brief Execute lighting pass
     * @param camera Current camera
     * @param point_lights Active point lights
     * @param spot_lights Active spot lights
     * @param sun_direction Sun direction
     * @param sun_color Sun color
     */
    void execute_lighting_pass(const Camera& camera, const std::vector<GPUPointLight>& point_lights,
                               const std::vector<GPUSpotLight>& spot_lights,
                               const glm::vec3& sun_direction, const glm::vec3& sun_color,
                               u32 irradiance_map = 0, u32 prefilter_map = 0, u32 brdf_lut = 0,
                               u32 environment_map = 0);

    /**
     * @brief Execute SSR pass
     * @param camera Current camera
     */
    void execute_ssr_pass(const Camera& camera);

    /**
     * @brief Execute TAA pass
     */
    void execute_taa_pass();

    /**
     * @brief Get jittered projection matrix for current TAA sample
     * Use this for the geometry pass projection when TAA is enabled.
     */
    [[nodiscard]] glm::mat4 get_taa_jittered_projection(const glm::mat4& proj) const {
        return m_taa.config.enabled ? m_taa.get_jittered_projection(proj) : proj;
    }

    /**
     * @brief Execute full post-processing chain
     * @param camera Current camera
     * @param exposure HDR exposure
     * @param bloom_threshold Bloom brightness threshold
     * @param bloom_intensity Bloom intensity
     */
    void execute_post_process(const Camera& camera, f32 exposure, f32 bloom_threshold,
                              f32 bloom_intensity);

    /**
     * @brief Render final output to screen
     */
    void render_to_screen();

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

    /**
     * @brief Get G-Buffer textures for debug visualization
     */
    [[nodiscard]] u32 get_gbuffer_albedo() const {
        return m_gbuffer.color_textures[GBUFFER_ALBEDO_METALLIC];
    }
    [[nodiscard]] u32 get_gbuffer_normal() const {
        return m_gbuffer.color_textures[GBUFFER_NORMAL_ROUGHNESS];
    }
    [[nodiscard]] u32 get_gbuffer_emission() const {
        return m_gbuffer.color_textures[GBUFFER_EMISSION_ID];
    }
    [[nodiscard]] u32 get_gbuffer_depth() const { return m_gbuffer.depth_texture; }
    [[nodiscard]] u32 get_shadow_map() const { return m_csm.depth_array_texture; }
    [[nodiscard]] u32 get_ssr_result() const { return m_ssr.color_texture; }
    [[nodiscard]] u32 get_final_output() const;

    // =========================================================================
    // Frustum Culling
    // =========================================================================

    /**
     * @brief Update frustum planes from camera
     */
    void update_frustum(const Camera& camera);

    /**
     * @brief Test if AABB is visible
     */
    [[nodiscard]] bool is_visible(const glm::vec3& min, const glm::vec3& max) const;

private:
    void create_shaders();
    void create_fullscreen_quad();
    void render_fullscreen_quad() const;

    // Dimensions
    u32 m_width{0};
    u32 m_height{0};

    // Pipeline stages
    GBuffer m_gbuffer;
    CascadedShadowMap m_csm;
    SSRPass m_ssr;
    TAAPass m_taa;

    // HDR + Post-process FBOs
    u32 m_lighting_fbo{0};
    u32 m_lighting_texture{0};
    u32 m_bloom_fbo{0};
    u32 m_bloom_texture{0};
    std::array<u32, 2> m_blur_fbos{}; // Ping-pong
    std::array<u32, 2> m_blur_textures{};
    u32 m_final_fbo{0};
    u32 m_final_texture{0};

    // Shaders
    std::unique_ptr<gl::Shader> m_geometry_shader;
    std::unique_ptr<gl::Shader> m_lighting_shader;
    std::unique_ptr<gl::Shader> m_shadow_shader;
    std::unique_ptr<gl::Shader> m_ssr_shader;
    std::unique_ptr<gl::Shader> m_taa_shader;
    std::unique_ptr<gl::Shader> m_bloom_extract_shader;
    std::unique_ptr<gl::Shader> m_blur_shader;
    std::unique_ptr<gl::Shader> m_composite_shader;

    // Fullscreen quad
    u32 m_quad_vao{0};
    u32 m_quad_vbo{0};

    // Frustum planes
    std::array<glm::vec4, 6> m_frustum_planes;

    // Shadow state (single directional shadow map for now)
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
inline glm::vec2 encode_octahedron(const glm::vec3& n) {
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
inline glm::vec3 decode_octahedron(const glm::vec2& f) {
    glm::vec2 f2 = f * 2.0f - 1.0f;
    glm::vec3 n(f2.x, f2.y, 1.0f - std::abs(f2.x) - std::abs(f2.y));
    f32 t = glm::clamp(-n.z, 0.0f, 1.0f);
    n.x += n.x >= 0.0f ? -t : t;
    n.y += n.y >= 0.0f ? -t : t;
    return glm::normalize(n);
}

} // namespace hz
