#include "renderer.hpp"

#include "engine/core/log.hpp"
#include "opengl/framebuffer.hpp"
#include "opengl/gl_context.hpp"
#include "opengl/shader.hpp"
#include "opengl/uniform_buffer.hpp"

#include <random>
#include <string>

#include <glm/gtc/matrix_transform.hpp>

namespace hz {

Renderer::Renderer(Window& window) : m_window(&window) {

    // Initialize OpenGL context
    if (!gl::init_context()) {
        throw std::runtime_error("Failed to initialize OpenGL context");
    }

    // Set default OpenGL state
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    // Enable blending
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Set initial viewport
    auto [width, height] = m_window->framebuffer_size();
    glViewport(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height));

    // Set clear color
    glClearColor(m_clear_color.r, m_clear_color.g, m_clear_color.b, m_clear_color.a);

    HZ_ENGINE_INFO("OpenGL Renderer initialized");

    init_quad();
    init_ssao();
    init_ubos();
}

Renderer::~Renderer() {
    if (m_ssao_noise_texture)
        glDeleteTextures(1, &m_ssao_noise_texture);
    HZ_ENGINE_INFO("OpenGL Renderer destroyed");
}

void Renderer::begin_frame() {
    // Update viewport if window was resized
    auto [width, height] = m_window->framebuffer_size();
    glViewport(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height));

    // Clear buffers
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Renderer::end_frame() {
    // Swap buffers is handled by GLFW in window
    m_window->swap_buffers();
}

std::pair<u32, u32> Renderer::framebuffer_size() const {
    return m_window->framebuffer_size();
}

void Renderer::set_clear_color(f32 r, f32 g, f32 b, f32 a) {
    m_clear_color = {r, g, b, a};
    glClearColor(r, g, b, a);
}

void Renderer::set_clear_color(const glm::vec4& color) {
    set_clear_color(color.r, color.g, color.b, color.a);
}

void Renderer::set_depth_test(bool enabled) {
    if (enabled) {
        glEnable(GL_DEPTH_TEST);
    } else {
        glDisable(GL_DEPTH_TEST);
    }
}

void Renderer::set_face_culling(bool enabled) {
    if (enabled) {
        glEnable(GL_CULL_FACE);
    } else {
        glDisable(GL_CULL_FACE);
    }
}

void Renderer::set_viewport(i32 x, i32 y, i32 width, i32 height) {
    glViewport(x, y, width, height);
}

void Renderer::submit_lighting(const SceneLighting& lighting) {
    m_scene_lighting = lighting;
}

void Renderer::apply_lighting(gl::Shader& shader) {
    // Bind UBOs
    shader.bind_uniform_block("CameraData", 0);
    shader.bind_uniform_block("SceneData", 1);

    // Lights are now handled by UBO (SceneData)
    // We only need to set shadow map matrix if valid

    // Shadows
    if (m_shadow_settings.enabled) {
        shader.set_mat4("u_light_space_matrix", m_shadow_settings.light_space_matrix);
    }
}

void Renderer::set_shadow_settings(const ShadowSettings& settings) {
    m_shadow_settings = settings;

    // Recreate FBO if resolution changed or not created
    if (!m_shadow_fbo || m_shadow_fbo->config().width != settings.resolution) {
        gl::FramebufferConfig fbo_config;
        fbo_config.width = settings.resolution;
        fbo_config.height = settings.resolution;
        fbo_config.depth_only = true;
        m_shadow_fbo = std::make_unique<gl::Framebuffer>(fbo_config);
    }
}

void Renderer::begin_shadow_pass() {
    if (!m_shadow_settings.enabled || !m_shadow_fbo)
        return;

    m_shadow_fbo->bind();

    // Update light space matrix for this frame
    m_shadow_settings.light_space_matrix = get_light_space_matrix();

    glClear(GL_DEPTH_BUFFER_BIT);
    // Cull front faces to fix peter panning
    // glCullFace(GL_FRONT); // TESTING: Disable to see if this hides shadows
}

void Renderer::end_shadow_pass() {
    if (!m_shadow_settings.enabled || !m_shadow_fbo)
        return;

    // Restore culling
    // glCullFace(GL_BACK);
    m_shadow_fbo->unbind();

    // Restore viewport
    auto [width, height] = m_window->framebuffer_size();
    glViewport(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height));
}

glm::mat4 Renderer::get_light_space_matrix() const {
    if (!m_shadow_settings.enabled)
        return glm::mat4(1.0f);

    float near_plane = m_shadow_settings.near_plane;
    float far_plane = m_shadow_settings.far_plane;
    float size = m_shadow_settings.ortho_size;

    glm::mat4 light_projection = glm::ortho(-size, size, -size, size, near_plane, far_plane);

    // Directional light position (Sun)
    // We assume the light is coming from 'direction', so we place the "camera"
    // at some offset along the inverse direction looking at origin (or player).
    // Ideally this follows the camera, but for static scene, this is fine.
    glm::vec3 light_dir = -glm::normalize(m_scene_lighting.sun.direction);
    glm::vec3 light_pos = light_dir * (far_plane / 2.0f); // Arbitrary distance

    // Use a fixed up vector to avoid gimbal lock if light is straight up
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
    if (glm::abs(glm::dot(light_dir, up)) > 0.9f) {
        up = glm::vec3(1.0f, 0.0f, 0.0f);
    }

    glm::mat4 light_view = glm::lookAt(light_pos, glm::vec3(0.0f), up);

    return light_projection * light_view;
}

void Renderer::bind_shadow_map(u32 slot) const {
    if (m_shadow_fbo) {
        glActiveTexture(GL_TEXTURE0 + slot);
        glBindTexture(GL_TEXTURE_2D, m_shadow_fbo->get_texture_id());
    }
}

// ==========================================
// Post-Processing (HDR)
// ==========================================

void Renderer::resize(u32 width, u32 height) {
    glViewport(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height));

    // Recreate HDR FBO
    gl::FramebufferConfig config;
    config.width = width;
    config.height = height;
    config.hdr = true;
    m_hdr_fbo = std::make_unique<gl::Framebuffer>(config);

    // Recreate G-Buffer FBO (Used for SSAO Prepass)
    gl::FramebufferConfig gbuffer_config;
    gbuffer_config.width = width;
    gbuffer_config.height = height;
    gbuffer_config.hdr = true;            // Use RGB16F for Normals
    gbuffer_config.depth_sampling = true; // We need depth for SSAO
    m_gbuffer_fbo = std::make_unique<gl::Framebuffer>(gbuffer_config);

    // SSAO FBO (Single Channel RED)
    // For performance, SSAO can be half-res? Let's stick to full res for quality first.
    gl::FramebufferConfig ssao_config;
    ssao_config.width = width;
    ssao_config.height = height;
    // We want a simple RED texture. Not HDR.
    // Framebuffer class defaults to RGB. We might need a "Red Only" flag or just use RGB and ignore
    // GB. Let's us RGB for now to keep Framebuffer class simple.
    m_ssao_fbo = std::make_unique<gl::Framebuffer>(ssao_config);
    m_ssao_blur_fbo = std::make_unique<gl::Framebuffer>(ssao_config); // Same config
}

void Renderer::begin_scene_pass() {
    if (!m_hdr_fbo) {
        auto [w, h] = m_window->framebuffer_size();
        resize(w, h);
    }
    m_hdr_fbo->bind();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Renderer::end_scene_pass() {
    if (m_hdr_fbo)
        m_hdr_fbo->unbind();
}

void Renderer::begin_geometry_pass() {
    if (!m_gbuffer_fbo) {
        auto [w, h] = m_window->framebuffer_size();
        resize(w, h); // This will create it
    }
    m_gbuffer_fbo->bind();
    // Clear Color (Normals) and Depth
    // We clear normals to 0,0,0 (or view direction?) 0,0,0 is safe.
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Renderer::end_geometry_pass() {
    if (m_gbuffer_fbo)
        m_gbuffer_fbo->unbind();
}

u32 Renderer::get_gbuffer_normal_texture() const {
    return m_gbuffer_fbo ? m_gbuffer_fbo->get_texture_id() : 0;
}

u32 Renderer::get_gbuffer_depth_texture() const {
    return m_gbuffer_fbo ? m_gbuffer_fbo->get_depth_texture_id() : 0;
}

u32 Renderer::get_shadow_map_texture_id() const {
    return m_shadow_fbo ? m_shadow_fbo->get_texture_id() : 0;
}

// ==========================================
// SSAO
// ==========================================

// Linear Interpolation Helper
static float lerp(float a, float b, float f) {
    return a + f * (b - a);
}

void Renderer::init_ssao() {
    std::uniform_real_distribution<float> randomFloats(
        0.0f, 1.0f); // generates random floats between 0.0 and 1.0
    std::default_random_engine generator;

    // 1. Generate Kernel
    for (unsigned int i = 0; i < 64; ++i) {
        glm::vec3 sample(randomFloats(generator) * 2.0 - 1.0, randomFloats(generator) * 2.0 - 1.0,
                         randomFloats(generator));
        sample = glm::normalize(sample);
        sample *= randomFloats(generator);

        float scale = float(i) / 64.0f;

        // Scale samples so they're more aligned to center of kernel
        scale = lerp(0.1f, 1.0f, scale * scale);
        sample *= scale;
        m_ssao_kernel.push_back(sample);
    }

    // 2. Generate Noise Texture
    std::vector<glm::vec3> ssaoNoise;
    for (unsigned int i = 0; i < 16; i++) {
        glm::vec3 noise(randomFloats(generator) * 2.0 - 1.0, randomFloats(generator) * 2.0 - 1.0,
                        0.0f); // rotate around z-axis (in tangent space)
        ssaoNoise.push_back(noise);
    }

    glGenTextures(1, &m_ssao_noise_texture);
    glBindTexture(GL_TEXTURE_2D, m_ssao_noise_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 4, 4, 0, GL_RGB, GL_FLOAT, &ssaoNoise[0]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
}

void Renderer::render_ssao(const gl::Shader& ssao_shader, const glm::mat4& projection) {
    if (!m_gbuffer_fbo)
        return;

    if (!m_ssao_fbo) {
        auto [w, h] = m_window->framebuffer_size();
        resize(w, h);
    }

    m_ssao_fbo->bind();
    glClear(GL_COLOR_BUFFER_BIT); // Clear occlusion to 0? Or 1?
    // Shader outputs occlusion factor.

    ssao_shader.bind();

    // Upload Kernel
    for (unsigned int i = 0; i < 64; ++i) {
        ssao_shader.set_vec3("u_samples[" + std::to_string(i) + "]", m_ssao_kernel[i]);
    }

    ssao_shader.set_mat4("u_projection", projection);
    // Inverse projection is needed for position reconstruction
    ssao_shader.set_mat4("u_inverse_projection", glm::inverse(projection));

    // Bind Textures
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, get_gbuffer_normal_texture()); // Normal
    ssao_shader.set_int("u_g_normal", 0);

    glActiveTexture(GL_TEXTURE0 + 1);
    glBindTexture(GL_TEXTURE_2D, get_gbuffer_depth_texture()); // Depth
    ssao_shader.set_int("u_g_depth", 1);

    glActiveTexture(GL_TEXTURE0 + 2);
    glBindTexture(GL_TEXTURE_2D, m_ssao_noise_texture); // Noise
    ssao_shader.set_int("u_tex_noise", 2);

    // Screen size for noise scaling
    auto [width, height] = m_window->framebuffer_size();
    ssao_shader.set_vec2("u_noise_scale", glm::vec2(width / 4.0f, height / 4.0f));

    if (!m_quad_vao)
        init_quad();
    glBindVertexArray(m_quad_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    m_ssao_fbo->unbind();
}

u32 Renderer::get_ssao_texture_id() const {
    return m_ssao_fbo ? m_ssao_fbo->get_texture_id() : 0;
}

void Renderer::render_ssao_blur(const gl::Shader& blur_shader) {
    if (!m_ssao_blur_fbo || !m_ssao_fbo)
        return;

    m_ssao_blur_fbo->bind();
    glClear(GL_COLOR_BUFFER_BIT);

    blur_shader.bind();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_ssao_fbo->get_texture_id());
    blur_shader.set_int("u_ssao_input", 0);

    if (!m_quad_vao)
        init_quad();
    glBindVertexArray(m_quad_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    m_ssao_blur_fbo->unbind();
}

u32 Renderer::get_ssao_blur_texture_id() const {
    return m_ssao_blur_fbo ? m_ssao_blur_fbo->get_texture_id() : 0;
}

void Renderer::render_post_process(const gl::Shader& hdr_shader) {
    if (!m_quad_vao)
        init_quad();

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // Clear default framebuffer

    hdr_shader.bind();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_hdr_fbo->get_texture_id());

    glBindVertexArray(m_quad_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

u32 Renderer::get_scene_texture_id() const {
    return m_hdr_fbo ? m_hdr_fbo->get_texture_id() : 0;
}

u32 Renderer::get_bloom_texture_id() const {
    // Return the final blurred result (pong buffer after even passes)
    return m_blur_fbo_pong ? m_blur_fbo_pong->get_texture_id() : 0;
}

void Renderer::render_bloom(gl::Shader& extract_shader, gl::Shader& blur_shader, float threshold,
                            int blur_passes) {
    if (!m_hdr_fbo)
        return;
    if (!m_quad_vao)
        init_quad();

    auto [width, height] = m_window->framebuffer_size();

    // Create bloom FBOs if needed (at half resolution for performance)
    u32 bloom_width = width / 2;
    u32 bloom_height = height / 2;

    if (!m_bloom_fbo || m_bloom_fbo->config().width != bloom_width) {
        gl::FramebufferConfig bloom_config;
        bloom_config.width = bloom_width;
        bloom_config.height = bloom_height;
        bloom_config.hdr = true;
        m_bloom_fbo = std::make_unique<gl::Framebuffer>(bloom_config);
        m_blur_fbo_ping = std::make_unique<gl::Framebuffer>(bloom_config);
        m_blur_fbo_pong = std::make_unique<gl::Framebuffer>(bloom_config);
    }

    glViewport(0, 0, static_cast<GLsizei>(bloom_width), static_cast<GLsizei>(bloom_height));

    // Pass 1: Extract bright pixels
    m_bloom_fbo->bind();
    glClear(GL_COLOR_BUFFER_BIT);
    extract_shader.bind();
    extract_shader.set_int("u_scene", 0);
    extract_shader.set_float("u_threshold", threshold);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_hdr_fbo->get_texture_id());
    glBindVertexArray(m_quad_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    m_bloom_fbo->unbind();

    // Pass 2+: Ping-pong Gaussian blur
    bool horizontal = true;
    bool first_iteration = true;
    blur_shader.bind();
    blur_shader.set_int("u_image", 0);

    for (int i = 0; i < blur_passes * 2; ++i) {
        if (horizontal) {
            m_blur_fbo_ping->bind();
        } else {
            m_blur_fbo_pong->bind();
        }
        glClear(GL_COLOR_BUFFER_BIT);
        blur_shader.set_bool("u_horizontal", horizontal);

        glActiveTexture(GL_TEXTURE0);
        if (first_iteration) {
            glBindTexture(GL_TEXTURE_2D, m_bloom_fbo->get_texture_id());
            first_iteration = false;
        } else {
            glBindTexture(GL_TEXTURE_2D, horizontal ? m_blur_fbo_pong->get_texture_id()
                                                    : m_blur_fbo_ping->get_texture_id());
        }

        glDrawArrays(GL_TRIANGLES, 0, 6);
        horizontal = !horizontal;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindVertexArray(0);

    // Restore viewport
    glViewport(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height));
}

void Renderer::init_quad() {
    float quad_vertices[] = {
        // positions   // texCoords
        -1.0f, 1.0f, 0.0f, 1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 1.0f, -1.0f, 1.0f, 0.0f,

        -1.0f, 1.0f, 0.0f, 1.0f, 1.0f,  -1.0f, 1.0f, 0.0f, 1.0f, 1.0f,  1.0f, 1.0f};

    glGenVertexArrays(1, &m_quad_vao);
    glGenBuffers(1, &m_quad_vbo);
    glBindVertexArray(m_quad_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_quad_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), &quad_vertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
}

void Renderer::render_texture(const gl::Shader& shader, u32 texture_id) {
    if (!m_quad_vao)
        init_quad();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    shader.bind();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture_id);

    // Assuming the shader uses slot 0 for the main texture
    // We cannot easily set uniforms here without knowing uniform names,
    // so we assume the caller sets uniforms or the shader defaults to slot 0.
    // For 'hdr_shader', it likely expects 'u_hdr_buffer' at slot 0.

    glBindVertexArray(m_quad_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

void Renderer::update_camera(const glm::mat4& view, const glm::mat4& projection,
                             const glm::vec3& view_pos) {
    if (!m_camera_ubo)
        return;

    CameraDataStd140 data;
    data.view = view;
    data.projection = projection;
    data.view_projection = projection * view;
    data.view_pos = glm::vec4(view_pos, 0.0f); // w unused

    auto [width, height] = m_window->framebuffer_size();
    data.viewport_size =
        glm::vec4(static_cast<float>(width), static_cast<float>(height), 0.0f, 0.0f);

    m_camera_ubo->set_data(data);
}

void Renderer::update_scene(float time) {
    if (!m_scene_ubo)
        return;

    SceneDataStd140 data{};

    // Sun
    data.sun.direction = glm::vec4(m_scene_lighting.sun.direction, 0.0f);
    data.sun.color = glm::vec4(m_scene_lighting.sun.color, 0.0f);
    data.sun.intensity = glm::vec4(m_scene_lighting.sun.intensity, 0.0f, 0.0f, 0.0f);

    // Ambient
    data.ambient_light = glm::vec4(m_scene_lighting.ambient_light, 0.0f);

    // Globals
    data.time = time;
    data.fog_enabled = 1;
    data.fog_density = 0.008f;
    data.fog_density = 0.008f;
    data.fog_gradient = 1.5f;
    data.fog_color = glm::vec4(m_clear_color); // Use clear color for fog

    // Point Lights
    int count = std::min(static_cast<int>(m_scene_lighting.point_lights.size()), 16);
    data.point_light_count = count;

    for (int i = 0; i < count; ++i) {
        const auto& light = m_scene_lighting.point_lights[static_cast<std::size_t>(i)];
        data.point_lights[i].position = glm::vec4(light.position, 1.0f);
        data.point_lights[i].color = glm::vec4(light.color, 1.0f);
        data.point_lights[i].intensity = light.intensity;
        data.point_lights[i].range = light.range;
    }

    m_scene_ubo->set_data(data);
}

void Renderer::init_ubos() {
    m_camera_ubo = std::make_unique<gl::UniformBuffer>(sizeof(CameraDataStd140), 0);
    m_scene_ubo = std::make_unique<gl::UniformBuffer>(sizeof(SceneDataStd140), 1);
}

} // namespace hz
