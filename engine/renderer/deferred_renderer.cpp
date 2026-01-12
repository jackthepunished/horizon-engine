#include "deferred_renderer.hpp"

#include "engine/core/log.hpp"
#include "engine/renderer/opengl/gl_context.hpp"

#include <cmath>

namespace hz {

// ============================================================================
// GBuffer Implementation
// ============================================================================

void GBuffer::create(u32 w, u32 h) {
    width = w;
    height = h;

    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    // Create color attachments
    glGenTextures(GBUFFER_COUNT, color_textures.data());

    for (u32 i = 0; i < GBUFFER_COUNT; ++i) {
        glBindTexture(GL_TEXTURE_2D, color_textures[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, static_cast<GLsizei>(width),
                     static_cast<GLsizei>(height), 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D,
                               color_textures[i], 0);
    }

    // Create depth texture
    glGenTextures(1, &depth_texture);
    glBindTexture(GL_TEXTURE_2D, depth_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, static_cast<GLsizei>(width),
                 static_cast<GLsizei>(height), 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth_texture, 0);

    // Set draw buffers for MRT - OpenGL 4.1 compatible
    GLenum attachments[GBUFFER_COUNT] = {GL_COLOR_ATTACHMENT0};
    // For OpenGL 4.1, iterate and set individually if glDrawBuffers not available
    // But OpenGL 4.1 DOES have glDrawBuffers - we need to update GLAD
    // For now, use single buffer approach
    glDrawBuffer(GL_COLOR_ATTACHMENT0);

    // Check completeness
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        HZ_ENGINE_ERROR("G-Buffer framebuffer incomplete!");
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    HZ_ENGINE_INFO("G-Buffer created: {}x{} (simplified single-target mode)", width, height);
}

void GBuffer::destroy() {
    if (fbo) {
        glDeleteFramebuffers(1, &fbo);
        glDeleteTextures(GBUFFER_COUNT, color_textures.data());
        glDeleteTextures(1, &depth_texture);
        fbo = 0;
        depth_texture = 0;
        color_textures = {};
    }
}

void GBuffer::bind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height));
}

void GBuffer::unbind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GBuffer::bind_textures(u32 start_slot) const {
    for (u32 i = 0; i < GBUFFER_COUNT; ++i) {
        glActiveTexture(GL_TEXTURE0 + start_slot + i);
        glBindTexture(GL_TEXTURE_2D, color_textures[i]);
    }
    glActiveTexture(GL_TEXTURE0 + start_slot + GBUFFER_COUNT);
    glBindTexture(GL_TEXTURE_2D, depth_texture);
}

// ============================================================================
// Cascaded Shadow Map Implementation (Using individual 2D textures)
// ============================================================================

void CascadedShadowMap::create(const CascadedShadowConfig& cfg) {
    config = cfg;

    // Create separate FBO and texture for each cascade
    // This is more compatible than texture arrays
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    // Create single depth texture (we'll switch between render passes)
    glGenTextures(1, &depth_array_texture);
    glBindTexture(GL_TEXTURE_2D, depth_array_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, static_cast<GLsizei>(config.resolution),
                 static_cast<GLsizei>(config.resolution), 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

    float border_color[] = {1.0f, 1.0f, 1.0f, 1.0f};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border_color);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth_array_texture,
                           0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        HZ_ENGINE_ERROR("CSM framebuffer incomplete!");
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    HZ_ENGINE_INFO("CSM created: {} cascades at {}x{} (single-texture mode)", config.cascade_count,
                   config.resolution, config.resolution);
}

void CascadedShadowMap::destroy() {
    if (fbo) {
        glDeleteFramebuffers(1, &fbo);
        glDeleteTextures(1, &depth_array_texture);
        fbo = 0;
        depth_array_texture = 0;
    }
}

void CascadedShadowMap::bind_cascade(u32 cascade_index) const {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, static_cast<GLsizei>(config.resolution),
               static_cast<GLsizei>(config.resolution));
}

void CascadedShadowMap::unbind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void CascadedShadowMap::update_cascades(const Camera& camera, const glm::vec3& light_dir) {
    calculate_cascade_splits(camera);

    for (u32 i = 0; i < config.cascade_count; ++i) {
        cascades[i].view_projection = calculate_light_space_matrix(i, camera, light_dir);
    }
}

void CascadedShadowMap::calculate_cascade_splits(const Camera& camera) {
    f32 near = camera.near_plane;
    f32 far = std::min(camera.far_plane, config.shadow_distance);
    f32 range = far - near;
    f32 ratio = far / near;

    for (u32 i = 0; i < config.cascade_count; ++i) {
        f32 p = static_cast<f32>(i + 1) / static_cast<f32>(config.cascade_count);
        f32 log_split = near * std::pow(ratio, p);
        f32 uniform_split = near + range * p;
        f32 d = config.split_lambda * (log_split - uniform_split) + uniform_split;
        cascades[i].split_depth = (d - near) / range;
    }
}

glm::mat4 CascadedShadowMap::calculate_light_space_matrix(u32 cascade, const Camera& camera,
                                                          const glm::vec3& light_dir) {
    f32 near = camera.near_plane;
    f32 far = std::min(camera.far_plane, config.shadow_distance);

    // Get cascade range
    f32 prev_split = cascade == 0 ? 0.0f : cascades[cascade - 1].split_depth;
    f32 split = cascades[cascade].split_depth;

    f32 cascade_near = near + (far - near) * prev_split;
    f32 cascade_far = near + (far - near) * split;

    // Calculate frustum corners for this cascade
    glm::mat4 proj =
        glm::perspective(glm::radians(camera.fov), 16.0f / 9.0f, cascade_near, cascade_far);
    glm::mat4 inv_cam = glm::inverse(proj * camera.view_matrix());

    std::array<glm::vec4, 8> frustum_corners = {glm::vec4(-1, -1, -1, 1), glm::vec4(1, -1, -1, 1),
                                                glm::vec4(1, 1, -1, 1),   glm::vec4(-1, 1, -1, 1),
                                                glm::vec4(-1, -1, 1, 1),  glm::vec4(1, -1, 1, 1),
                                                glm::vec4(1, 1, 1, 1),    glm::vec4(-1, 1, 1, 1)};

    // Transform corners to world space
    glm::vec3 center(0.0f);
    for (auto& corner : frustum_corners) {
        corner = inv_cam * corner;
        corner /= corner.w;
        center += glm::vec3(corner);
    }
    center /= 8.0f;

    // Light view matrix
    glm::mat4 light_view =
        glm::lookAt(center - glm::normalize(light_dir) * config.shadow_distance * 0.5f, center,
                    glm::vec3(0.0f, 1.0f, 0.0f));

    // Find bounding box in light space
    glm::vec3 min_bounds(std::numeric_limits<f32>::max());
    glm::vec3 max_bounds(std::numeric_limits<f32>::lowest());

    for (const auto& corner : frustum_corners) {
        glm::vec4 light_corner = light_view * corner;
        min_bounds = glm::min(min_bounds, glm::vec3(light_corner));
        max_bounds = glm::max(max_bounds, glm::vec3(light_corner));
    }

    // Snap to texel size to avoid shadow swimming
    f32 texel_size = (max_bounds.x - min_bounds.x) / static_cast<f32>(config.resolution);
    min_bounds.x = std::floor(min_bounds.x / texel_size) * texel_size;
    max_bounds.x = std::floor(max_bounds.x / texel_size) * texel_size;
    min_bounds.y = std::floor(min_bounds.y / texel_size) * texel_size;
    max_bounds.y = std::floor(max_bounds.y / texel_size) * texel_size;

    // Orthographic projection
    glm::mat4 light_proj = glm::ortho(min_bounds.x, max_bounds.x, min_bounds.y, max_bounds.y,
                                      -max_bounds.z - 50.0f, -min_bounds.z + 50.0f);

    return light_proj * light_view;
}

// ============================================================================
// SSR Pass Implementation
// ============================================================================

void SSRPass::create(u32 w, u32 h, const SSRConfig& cfg) {
    config = cfg;
    width = static_cast<u32>(static_cast<f32>(w) * config.resolution_scale);
    height = static_cast<u32>(static_cast<f32>(h) * config.resolution_scale);

    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    glGenTextures(1, &color_texture);
    glBindTexture(GL_TEXTURE_2D, color_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, static_cast<GLsizei>(width),
                 static_cast<GLsizei>(height), 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color_texture, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        HZ_ENGINE_ERROR("SSR FBO incomplete!");
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void SSRPass::destroy() {
    if (fbo) {
        glDeleteFramebuffers(1, &fbo);
        glDeleteTextures(1, &color_texture);
        fbo = 0;
        color_texture = 0;
    }
}

void SSRPass::bind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height));
}

void SSRPass::unbind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// ============================================================================
// TAA Pass Implementation
// ============================================================================

void TAAPass::create(u32 w, u32 h, const TAAConfig& cfg) {
    config = cfg;
    width = w;
    height = h;
    frame_index = 0;

    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    // Current frame texture
    glGenTextures(1, &current_texture);
    glBindTexture(GL_TEXTURE_2D, current_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, static_cast<GLsizei>(width),
                 static_cast<GLsizei>(height), 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, current_texture, 0);

    // History texture
    glGenTextures(1, &history_texture);
    glBindTexture(GL_TEXTURE_2D, history_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, static_cast<GLsizei>(width),
                 static_cast<GLsizei>(height), 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Velocity buffer
    glGenTextures(1, &velocity_texture);
    glBindTexture(GL_TEXTURE_2D, velocity_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, static_cast<GLsizei>(width),
                 static_cast<GLsizei>(height), 0, GL_RG, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        HZ_ENGINE_ERROR("TAA FBO incomplete!");
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    generate_halton_sequence();
}

void TAAPass::destroy() {
    if (fbo) {
        glDeleteFramebuffers(1, &fbo);
        glDeleteTextures(1, &current_texture);
        glDeleteTextures(1, &history_texture);
        glDeleteTextures(1, &velocity_texture);
        fbo = 0;
        current_texture = 0;
        history_texture = 0;
        velocity_texture = 0;
    }
}

void TAAPass::bind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height));
}

void TAAPass::unbind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void TAAPass::swap_history() {
    std::swap(current_texture, history_texture);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, current_texture, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    frame_index = (frame_index + 1) % JITTER_SAMPLE_COUNT;
}

glm::vec2 TAAPass::get_current_jitter() const {
    return jitter_offsets[frame_index];
}

glm::mat4 TAAPass::get_jittered_projection(const glm::mat4& proj) const {
    glm::vec2 jitter = get_current_jitter();
    glm::mat4 jittered = proj;
    jittered[2][0] += jitter.x * 2.0f / static_cast<f32>(width);
    jittered[2][1] += jitter.y * 2.0f / static_cast<f32>(height);
    return jittered;
}

void TAAPass::generate_halton_sequence() {
    auto halton = [](u32 index, u32 base) -> f32 {
        f32 f = 1.0f;
        f32 r = 0.0f;
        u32 i = index;
        while (i > 0) {
            f /= static_cast<f32>(base);
            r += f * static_cast<f32>(i % base);
            i /= base;
        }
        return r;
    };

    for (u32 i = 0; i < JITTER_SAMPLE_COUNT; ++i) {
        jitter_offsets[i] = glm::vec2(halton(i + 1, 2) - 0.5f, halton(i + 1, 3) - 0.5f);
    }
}

// ============================================================================
// Deferred Renderer Implementation
// ============================================================================

DeferredRenderer::~DeferredRenderer() {
    shutdown();
}

bool DeferredRenderer::init(u32 width, u32 height) {
    if (m_initialized)
        return true;

    m_width = width;
    m_height = height;

    // Create G-Buffer
    m_gbuffer.create(width, height);

    // Create Cascaded Shadow Maps
    CascadedShadowConfig csm_config;
    csm_config.cascade_count = 4;
    csm_config.resolution = 2048;
    m_csm.create(csm_config);

    // Create SSR pass
    SSRConfig ssr_config;
    m_ssr.create(width, height, ssr_config);

    // Create TAA pass
    TAAConfig taa_config;
    m_taa.create(width, height, taa_config);

    // Create lighting FBO
    glGenFramebuffers(1, &m_lighting_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_lighting_fbo);

    glGenTextures(1, &m_lighting_texture);
    glBindTexture(GL_TEXTURE_2D, m_lighting_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, static_cast<GLsizei>(width),
                 static_cast<GLsizei>(height), 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_lighting_texture,
                           0);

    // Create bloom FBOs (ping-pong)
    glGenFramebuffers(1, &m_bloom_fbo);
    glGenTextures(1, &m_bloom_texture);
    glBindFramebuffer(GL_FRAMEBUFFER, m_bloom_fbo);
    glBindTexture(GL_TEXTURE_2D, m_bloom_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, static_cast<GLsizei>(width / 2),
                 static_cast<GLsizei>(height / 2), 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_bloom_texture, 0);

    // Blur ping-pong
    glGenFramebuffers(2, m_blur_fbos.data());
    glGenTextures(2, m_blur_textures.data());
    for (u32 i = 0; i < 2; ++i) {
        glBindFramebuffer(GL_FRAMEBUFFER, m_blur_fbos[i]);
        glBindTexture(GL_TEXTURE_2D, m_blur_textures[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, static_cast<GLsizei>(width / 2),
                     static_cast<GLsizei>(height / 2), 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                               m_blur_textures[i], 0);
    }

    // Final output FBO
    glGenFramebuffers(1, &m_final_fbo);
    glGenTextures(1, &m_final_texture);
    glBindFramebuffer(GL_FRAMEBUFFER, m_final_fbo);
    glBindTexture(GL_TEXTURE_2D, m_final_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, static_cast<GLsizei>(width),
                 static_cast<GLsizei>(height), 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_final_texture, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Create fullscreen quad
    create_fullscreen_quad();

    m_initialized = true;
    HZ_ENGINE_INFO("Deferred Renderer initialized: {}x{}", width, height);
    return true;
}

void DeferredRenderer::shutdown() {
    if (!m_initialized)
        return;

    m_gbuffer.destroy();
    m_csm.destroy();
    m_ssr.destroy();
    m_taa.destroy();

    if (m_lighting_fbo) {
        glDeleteFramebuffers(1, &m_lighting_fbo);
        glDeleteTextures(1, &m_lighting_texture);
    }
    if (m_bloom_fbo) {
        glDeleteFramebuffers(1, &m_bloom_fbo);
        glDeleteTextures(1, &m_bloom_texture);
    }
    glDeleteFramebuffers(2, m_blur_fbos.data());
    glDeleteTextures(2, m_blur_textures.data());
    if (m_final_fbo) {
        glDeleteFramebuffers(1, &m_final_fbo);
        glDeleteTextures(1, &m_final_texture);
    }
    if (m_quad_vao) {
        glDeleteVertexArrays(1, &m_quad_vao);
        glDeleteBuffers(1, &m_quad_vbo);
    }

    m_initialized = false;
    HZ_ENGINE_INFO("Deferred Renderer shutdown");
}

void DeferredRenderer::resize(u32 width, u32 height) {
    if (width == m_width && height == m_height)
        return;

    m_width = width;
    m_height = height;

    // Recreate all render targets
    m_gbuffer.destroy();
    m_gbuffer.create(width, height);

    m_ssr.destroy();
    m_ssr.create(width, height, m_ssr.config);

    m_taa.destroy();
    m_taa.create(width, height, m_taa.config);

    HZ_ENGINE_INFO("Deferred Renderer resized: {}x{}", width, height);
}

void DeferredRenderer::begin_geometry_pass(const Camera& camera) {
    m_gbuffer.bind();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    // Update frustum for culling
    update_frustum(camera);
}

void DeferredRenderer::end_geometry_pass() {
    m_gbuffer.unbind();
}

void DeferredRenderer::render_shadows(const glm::vec3& light_direction) {
    glEnable(GL_DEPTH_TEST);
    glCullFace(GL_FRONT); // Peter panning fix

    for (u32 i = 0; i < m_csm.config.cascade_count; ++i) {
        m_csm.bind_cascade(i);
        glClear(GL_DEPTH_BUFFER_BIT);
    }

    glCullFace(GL_BACK);
    m_csm.unbind();
}

void DeferredRenderer::execute_lighting_pass(const Camera& camera,
                                             const std::vector<GPUPointLight>& point_lights,
                                             const std::vector<GPUSpotLight>& spot_lights,
                                             const glm::vec3& sun_direction,
                                             const glm::vec3& sun_color) {
    glBindFramebuffer(GL_FRAMEBUFFER, m_lighting_fbo);
    glViewport(0, 0, static_cast<GLsizei>(m_width), static_cast<GLsizei>(m_height));
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);

    m_gbuffer.bind_textures(0);

    glActiveTexture(GL_TEXTURE0 + GBUFFER_COUNT + 1);
    glBindTexture(GL_TEXTURE_2D, m_csm.depth_array_texture);

    render_fullscreen_quad();

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void DeferredRenderer::execute_ssr_pass(const Camera& camera) {
    if (!m_ssr.config.enabled)
        return;

    m_ssr.bind();
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);

    m_gbuffer.bind_textures(0);
    glActiveTexture(GL_TEXTURE0 + GBUFFER_COUNT + 1);
    glBindTexture(GL_TEXTURE_2D, m_lighting_texture);

    render_fullscreen_quad();
    m_ssr.unbind();
}

void DeferredRenderer::execute_taa_pass() {
    if (!m_taa.config.enabled)
        return;

    m_taa.bind();
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_lighting_texture);
    glActiveTexture(GL_TEXTURE0 + 1);
    glBindTexture(GL_TEXTURE_2D, m_taa.history_texture);
    glActiveTexture(GL_TEXTURE0 + 2);
    glBindTexture(GL_TEXTURE_2D, m_taa.velocity_texture);

    render_fullscreen_quad();
    m_taa.unbind();
    m_taa.swap_history();
}

void DeferredRenderer::execute_post_process(const Camera& camera, f32 exposure, f32 bloom_threshold,
                                            f32 bloom_intensity) {
    // Bloom extraction and blur would happen here
}

void DeferredRenderer::render_to_screen() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, static_cast<GLsizei>(m_width), static_cast<GLsizei>(m_height));
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_taa.config.enabled ? m_taa.current_texture : m_lighting_texture);

    render_fullscreen_quad();
}

void DeferredRenderer::update_frustum(const Camera& camera) {
    glm::mat4 vp =
        camera.projection_matrix(static_cast<f32>(m_width) / static_cast<f32>(m_height)) *
        camera.view_matrix();

    // Extract frustum planes (Gribb/Hartmann method)
    for (u32 i = 0; i < 3; ++i) {
        m_frustum_planes[i * 2] = glm::vec4(vp[0][3] + vp[0][i], vp[1][3] + vp[1][i],
                                            vp[2][3] + vp[2][i], vp[3][3] + vp[3][i]);
        m_frustum_planes[i * 2 + 1] = glm::vec4(vp[0][3] - vp[0][i], vp[1][3] - vp[1][i],
                                                vp[2][3] - vp[2][i], vp[3][3] - vp[3][i]);
    }

    // Normalize planes
    for (auto& plane : m_frustum_planes) {
        f32 len = glm::length(glm::vec3(plane));
        plane /= len;
    }
}

bool DeferredRenderer::is_visible(const glm::vec3& min, const glm::vec3& max) const {
    for (const auto& plane : m_frustum_planes) {
        glm::vec3 p(plane.x > 0 ? max.x : min.x, plane.y > 0 ? max.y : min.y,
                    plane.z > 0 ? max.z : min.z);
        if (glm::dot(glm::vec3(plane), p) + plane.w < 0) {
            return false;
        }
    }
    return true;
}

void DeferredRenderer::create_fullscreen_quad() {
    float quad_vertices[] = {
        // positions        // texcoords
        -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
        1.0f,  1.0f, 0.0f, 1.0f, 1.0f, 1.0f,  -1.0f, 0.0f, 1.0f, 0.0f,
    };

    glGenVertexArrays(1, &m_quad_vao);
    glGenBuffers(1, &m_quad_vbo);
    glBindVertexArray(m_quad_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_quad_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), quad_vertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                          reinterpret_cast<void*>(3 * sizeof(float)));
    glBindVertexArray(0);
}

void DeferredRenderer::render_fullscreen_quad() const {
    glBindVertexArray(m_quad_vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}

void DeferredRenderer::reset_stats() {
    m_stats = RenderStats{};
}

u32 DeferredRenderer::get_final_output() const {
    return m_taa.config.enabled ? m_taa.current_texture : m_lighting_texture;
}

void DeferredRenderer::set_csm_config(const CascadedShadowConfig& config) {
    m_csm.destroy();
    m_csm.create(config);
}

void DeferredRenderer::set_ssr_config(const SSRConfig& config) {
    m_ssr.destroy();
    m_ssr.create(m_width, m_height, config);
}

void DeferredRenderer::set_taa_config(const TAAConfig& config) {
    m_taa.destroy();
    m_taa.create(m_width, m_height, config);
}

} // namespace hz
