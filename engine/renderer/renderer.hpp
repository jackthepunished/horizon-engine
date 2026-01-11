#pragma once

/**
 * @file renderer.hpp
 * @brief High-level OpenGL renderer interface
 */

#include "engine/core/types.hpp"
#include "engine/platform/window.hpp"

#include <memory>
#include <vector>

#include <glm/glm.hpp>

namespace hz {

namespace gl {
class Shader;
class Framebuffer;
class UniformBuffer;
} // namespace gl

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
     * @brief Create a renderer for the given window
     */
    explicit Renderer(Window& window);

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
     * @brief Begin a new frame
     */
    void begin_frame();

    /**
     * @brief End the current frame and swap buffers
     */
    void end_frame();

    // ========================================================================
    // Lighting
    // ========================================================================

    /**
     * @brief Submit lighting environment for the current frame
     */
    void submit_lighting(const SceneLighting& lighting);

    /**
     * @brief Apply lighting uniforms to the given shader
     */
    void apply_lighting(gl::Shader& shader);

    // ========================================================================
    // Shadows
    // ========================================================================

    /**
     * @brief Configure shadows (must be called before shadow pass)
     */
    void set_shadow_settings(const ShadowSettings& settings);

    /**
     * @brief Begin shadow map rendering pass
     */
    void begin_shadow_pass();

    /**
     * @brief End shadow map rendering pass
     */
    void end_shadow_pass();

    /**
     * @brief Get the Light Space Matrix (Projection * View) used for shadows
     */
    [[nodiscard]] glm::mat4 get_light_space_matrix() const;

    /**
     * @brief Bind the shadow map texture to a specific slot
     */
    void bind_shadow_map(u32 slot = 1) const;

    // ========================================================================
    // Properties
    // ========================================================================

    /**
     * @brief Get the framebuffer size
     */
    [[nodiscard]] std::pair<u32, u32> framebuffer_size() const;

    /**
     * @brief Set the clear color
     */
    void set_clear_color(f32 r, f32 g, f32 b, f32 a = 1.0f);

    /**
     * @brief Set the clear color from vec4
     */
    void set_clear_color(const glm::vec4& color);

    /**
     * @brief Enable/disable depth testing
     */
    void set_depth_test(bool enabled);

    /**
     * @brief Enable/disable face culling
     */
    void set_face_culling(bool enabled);

    /**
     * @brief Set viewport
     */
    void set_viewport(i32 x, i32 y, i32 width, i32 height);

    // Post-Processing (HDR)
    void resize(u32 width, u32 height);                     // Handle window resize for FBOs
    void begin_scene_pass();                                // Renders to HDR FBO
    void end_scene_pass();                                  // Unbinds HDR FBO
    void render_post_process(const gl::Shader& hdr_shader); // Renders Quad to screen

    // Geometry Pass (SSAO Prepass)
    void begin_geometry_pass();
    void end_geometry_pass();
    [[nodiscard]] u32 get_gbuffer_normal_texture() const;
    [[nodiscard]] u32 get_gbuffer_depth_texture() const;

    // SSAO
    void init_ssao();
    void render_ssao(const gl::Shader& ssao_shader, const glm::mat4& projection);
    void render_ssao_blur(const gl::Shader& blur_shader);
    [[nodiscard]] u32 get_ssao_texture_id() const;
    [[nodiscard]] u32 get_ssao_blur_texture_id() const;

    // Bloom
    void render_bloom(gl::Shader& extract_shader, gl::Shader& blur_shader, float threshold,
                      int blur_passes);
    [[nodiscard]] u32 get_bloom_texture_id() const;

    void render_texture(const gl::Shader& shader, u32 texture_id);

    [[nodiscard]] u32 get_scene_texture_id() const;
    [[nodiscard]] u32 get_shadow_map_texture_id() const;

    // UBOs
    void update_camera(const glm::mat4& view, const glm::mat4& projection,
                       const glm::vec3& view_pos);
    void update_scene(float time);

private:
    void init_quad();
    void init_ubos();

    Window* m_window;
    glm::vec4 m_clear_color{0.1f, 0.1f, 0.15f, 1.0f};
    SceneLighting m_scene_lighting;

    std::unique_ptr<gl::UniformBuffer> m_camera_ubo;
    std::unique_ptr<gl::UniformBuffer> m_scene_ubo;

    // Shadows
    std::unique_ptr<gl::Framebuffer> m_shadow_fbo;
    ShadowSettings m_shadow_settings;

    // HDR
    std::unique_ptr<gl::Framebuffer> m_hdr_fbo;
    u32 m_quad_vao{0};
    u32 m_quad_vbo{0};

    // Bloom
    std::unique_ptr<gl::Framebuffer> m_bloom_fbo;     // Brightness extraction
    std::unique_ptr<gl::Framebuffer> m_blur_fbo_ping; // Ping-pong blur
    std::unique_ptr<gl::Framebuffer> m_blur_fbo_pong;

    // SSAO / G-Buffer
    std::unique_ptr<gl::Framebuffer> m_gbuffer_fbo;
    std::unique_ptr<gl::Framebuffer> m_ssao_fbo;
    std::unique_ptr<gl::Framebuffer> m_ssao_blur_fbo; // Reuse blur FBO? No, need single channel.

    std::vector<glm::vec3> m_ssao_kernel;
    u32 m_ssao_noise_texture{0};
};

} // namespace hz
