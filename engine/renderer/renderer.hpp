#pragma once

/**
 * @file renderer.hpp
 * @brief High-level OpenGL renderer interface
 */

#include "engine/core/types.hpp"
#include "engine/platform/window.hpp"
#include "engine/rhi/rhi_command_list.hpp"
#include "engine/rhi/rhi_device.hpp"
#include "engine/rhi/rhi_pipeline.hpp"
#include "engine/rhi/rhi_resources.hpp"

#include <memory>
#include <vector>

#include <glm/glm.hpp>

namespace hz {
// GL namespace removed

struct DirectionalLight {
    glm::vec3 direction{0.0f, -1.0f, 0.0f};
    glm::vec3 color{1.0f};
    float intensity{1.0f};
};

struct PointLight {
    glm::vec3 position{0.0f};
    glm::vec3 color{1.0f};
    float intensity{1.0f};
    float range{10.0f};
};

struct SpotLight {
    glm::vec3 position{0.0f};
    glm::vec3 direction{0.0f, -1.0f, 0.0f};
    glm::vec3 color{1.0f};
    float intensity{1.0f};
    float range{10.0f};
    float cut_off{0.976f};       // ~12.5 deg
    float outer_cut_off{0.953f}; // ~17.5 deg
};

struct SceneLighting {
    DirectionalLight sun;
    std::vector<PointLight> point_lights;
    std::vector<SpotLight> spot_lights;
    glm::vec3 ambient_light{0.1f};
};

struct ShadowSettings {
    bool enabled{true};
    u32 resolution{2048};
    float ortho_size{20.0f};
    float near_plane{1.0f};
    float far_plane{50.0f};
    glm::vec3 light_pos_offset{-10.0f, 20.0f, -10.0f};
    glm::mat4 light_space_matrix;
};

// std140 compatible structs for UBOs
struct CameraDataStd140 {
    glm::mat4 view;
    glm::mat4 projection;
    glm::mat4 view_projection;
    glm::vec4 view_pos;      // xyz = pos, w = pad
    glm::vec4 viewport_size; // xy = size, zw = pad
};

struct DirectionalLightStd140 {
    glm::vec4 direction; // w = pad
    glm::vec4 color;     // w = pad
    glm::vec4 intensity; // x = intensity, yzw = pad
};

struct PointLightStd140 {
    glm::vec4 position; // w = pad
    glm::vec4 color;    // w = pad
    float intensity;
    float range;
    float pad[2]; // total 48 bytes
};

struct SceneDataStd140 {
    DirectionalLightStd140 sun; // 48
    glm::vec4 ambient_light;    // 16
    float time;
    int fog_enabled;
    float fog_density;
    float fog_gradient;
    glm::vec4 fog_color;
    int point_light_count;
    float pad[3]; // align to 16 bytes for array start?
                  // Offsets: sun(48), ambient(64), time(68), foge(72), fogd(76), fogg(80),
                  // fogc(80->96), plc(96) Next multiple of 16 for array is 112. plc(96)+4=100. Diff
                  // = 12 (3 floats).
    PointLightStd140 point_lights[16];
};

/**
 * @brief OpenGL Renderer
 *
 * Manages the render loop with clear-screen rendering.
 */
class Renderer {
public:
    /**
     * @brief Create a renderer
     * @param device RHI Device
     * @param swapchain RHI Swapchain
     */
    Renderer(rhi::Device& device, rhi::Swapchain& swapchain);

    /**
     * @brief Destroy the renderer
     */
    ~Renderer() noexcept;

    HZ_NON_COPYABLE(Renderer);
    HZ_NON_MOVABLE(Renderer);

    // ========================================================================
    // Frame Lifecycle
    // ========================================================================

    /**
     * @brief Begin a new frame (acquires image)
     */
    void begin_frame();

    /**
     * @brief End the current frame and present
     */
    void end_frame();

    /**
     * @brief Get the current command list for recording
     */
    [[nodiscard]] rhi::CommandList* get_command_list() const;

    // ========================================================================
    // Lighting
    // ========================================================================

    void submit_lighting(const SceneLighting& lighting);

    // ========================================================================
    // Shadows
    // ========================================================================

    void set_shadow_settings(const ShadowSettings& settings);

    void begin_shadow_pass(rhi::CommandList& cmd);
    void end_shadow_pass(rhi::CommandList& cmd);

    [[nodiscard]] glm::mat4 get_light_space_matrix() const;
    [[nodiscard]] rhi::TextureView* get_shadow_map_view() const;

    // ========================================================================
    // Properties
    // ========================================================================

    [[nodiscard]] std::pair<u32, u32> framebuffer_size() const;

    void set_clear_color(f32 r, f32 g, f32 b, f32 a = 1.0f);
    void set_clear_color(const glm::vec4& color);

    void set_viewport(rhi::CommandList& cmd, i32 x, i32 y, i32 width, i32 height);

    // Post-Processing (HDR)
    void resize(u32 width, u32 height);

    // Pass Management
    void begin_scene_pass(rhi::CommandList& cmd);
    void end_scene_pass(rhi::CommandList& cmd);

    void render_post_process(rhi::CommandList& cmd);

    // Geometry Pass (SSAO Prepass)
    void begin_geometry_pass(rhi::CommandList& cmd);
    void end_geometry_pass(rhi::CommandList& cmd);

    [[nodiscard]] rhi::TextureView* get_gbuffer_normal_view() const;
    [[nodiscard]] rhi::TextureView* get_gbuffer_depth_view() const;

    // SSAO
    void init_ssao();
    void render_ssao(rhi::CommandList& cmd, const glm::mat4& projection);
    void render_ssao_blur(rhi::CommandList& cmd);

    [[nodiscard]] rhi::TextureView* get_ssao_view() const;

    // Bloom
    void render_bloom(rhi::CommandList& cmd, float threshold, int blur_passes);
    [[nodiscard]] rhi::TextureView* get_bloom_view() const;

    // UBOs
    void update_camera(const glm::mat4& view, const glm::mat4& projection,
                       const glm::vec3& view_pos);
    void update_scene(float time);

private:
    void init_quad();
    void init_ubos();
    void create_resources();

    rhi::Device& m_device;
    rhi::Swapchain& m_swapchain;

    // Command Buffer for the current frame
    rhi::CommandList* m_current_cmd{nullptr};

    glm::vec4 m_clear_color{0.1f, 0.1f, 0.15f, 1.0f};
    SceneLighting m_scene_lighting;

    // UBOs (mapped buffers)
    std::unique_ptr<rhi::Buffer> m_camera_ubo;
    std::unique_ptr<rhi::Buffer> m_scene_ubo;

    // Shadows
    std::unique_ptr<rhi::Framebuffer> m_shadow_fbo;
    std::unique_ptr<rhi::Texture> m_shadow_map;
    std::unique_ptr<rhi::TextureView> m_shadow_map_view;
    ShadowSettings m_shadow_settings;

    // HDR
    std::unique_ptr<rhi::Framebuffer> m_hdr_fbo;
    std::unique_ptr<rhi::Texture> m_hdr_color_texture;
    std::unique_ptr<rhi::TextureView> m_hdr_color_view;
    std::unique_ptr<rhi::Texture> m_hdr_depth_texture;
    std::unique_ptr<rhi::TextureView> m_hdr_depth_view;

    // Quad Resources (Vertex Buffer)
    std::unique_ptr<rhi::Buffer> m_quad_vb;

    // Bloom
    std::unique_ptr<rhi::Framebuffer> m_bloom_fbo;
    std::unique_ptr<rhi::Framebuffer> m_blur_fbo_ping;
    std::unique_ptr<rhi::Framebuffer> m_blur_fbo_pong;

    // SSAO / G-Buffer
    std::unique_ptr<rhi::Framebuffer> m_gbuffer_fbo;
    std::unique_ptr<rhi::Framebuffer> m_ssao_fbo;
    std::unique_ptr<rhi::Framebuffer> m_ssao_blur_fbo;

    std::vector<glm::vec3> m_ssao_kernel;
    std::unique_ptr<rhi::Texture> m_ssao_noise_texture;
    std::unique_ptr<rhi::TextureView> m_ssao_noise_view;

    // Volumetric Fog
    std::unique_ptr<rhi::Framebuffer> m_volumetric_fbo;
};

} // namespace hz
