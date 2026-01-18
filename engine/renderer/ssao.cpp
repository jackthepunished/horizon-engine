#include "ssao.hpp"

#include "engine/core/log.hpp"
#include "engine/renderer/opengl/shader.hpp"
#include "engine/renderer/renderer.hpp" // For shader management if needed

#include <random>

#include <glad/glad.h>

namespace hz {

SSAO::~SSAO() {
    destroy();
}

void SSAO::create(u32 width, u32 height, const SSAOConfig& cfg) {
    config = cfg;
    resize(width, height);
    generate_kernel();
    generate_noise();
}

void SSAO::destroy() {
    if (m_fbo) {
        glDeleteFramebuffers(1, &m_fbo);
        glDeleteTextures(1, &m_color_texture);
        m_fbo = 0;
        m_color_texture = 0;
    }
    if (m_blur_fbo) {
        glDeleteFramebuffers(1, &m_blur_fbo);
        glDeleteTextures(1, &m_blur_texture);
        m_blur_fbo = 0;
        m_blur_texture = 0;
    }
    if (m_noise_texture) {
        glDeleteTextures(1, &m_noise_texture);
        m_noise_texture = 0;
    }
}

void SSAO::resize(u32 width, u32 height) {
    m_width = static_cast<u32>(width * config.resolution_scale);
    m_height = static_cast<u32>(height * config.resolution_scale);
    init_framebuffers(m_width, m_height);
}

void SSAO::init_framebuffers(u32 width, u32 height) {
    // Cleanup old FBOs if they exist (handling resize)
    if (m_fbo) {
        glDeleteFramebuffers(1, &m_fbo);
        glDeleteTextures(1, &m_color_texture);
    }
    if (m_blur_fbo) {
        glDeleteFramebuffers(1, &m_blur_fbo);
        glDeleteTextures(1, &m_blur_texture);
    }

    // SSAO FBO
    glGenFramebuffers(1, &m_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

    glGenTextures(1, &m_color_texture);
    glBindTexture(GL_TEXTURE_2D, m_color_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, static_cast<GLsizei>(width), static_cast<GLsizei>(height),
                 0, GL_RED, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_color_texture, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        HZ_ENGINE_ERROR("SSAO FBO Incomplete");

    // Blur FBO
    glGenFramebuffers(1, &m_blur_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_blur_fbo);

    glGenTextures(1, &m_blur_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, static_cast<GLsizei>(width), static_cast<GLsizei>(height),
                 0, GL_RED, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_blur_texture, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        HZ_ENGINE_ERROR("SSAO Blur FBO Incomplete");

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void SSAO::generate_kernel() {
    std::uniform_real_distribution<float> random_floats(0.0, 1.0);
    std::default_random_engine generator;

    m_kernel.clear();
    for (int i = 0; i < config.kernel_size; ++i) {
        glm::vec3 sample(random_floats(generator) * 2.0 - 1.0, random_floats(generator) * 2.0 - 1.0,
                         random_floats(generator) // Hemisphere Z > 0
        );
        sample = glm::normalize(sample);
        sample *= random_floats(generator); // Randomize length

        // Scale samples to cluster near origin (center of kernel)
        float scale = float(i) / float(config.kernel_size);
        scale = 0.1f + (scale * scale) * 0.9f; // Lerp: 0.1 -> 1.0
        sample *= scale;

        m_kernel.push_back(sample);
    }
}

void SSAO::generate_noise() {
    std::uniform_real_distribution<float> random_floats(0.0, 1.0);
    std::default_random_engine generator;

    m_noise.clear();
    for (unsigned int i = 0; i < 16; i++) {
        glm::vec3 noise(random_floats(generator) * 2.0 - 1.0, random_floats(generator) * 2.0 - 1.0,
                        0.0f);
        m_noise.push_back(noise);
    }

    if (m_noise_texture)
        glDeleteTextures(1, &m_noise_texture);

    glGenTextures(1, &m_noise_texture);
    glBindTexture(GL_TEXTURE_2D, m_noise_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, 4, 4, 0, GL_RGB, GL_FLOAT, &m_noise[0]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT); // Repeat noise
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
}

void SSAO::render(u32 g_position, u32 g_normal, const glm::mat4& projection,
                  const gl::Shader& ssao_shader, const gl::Shader& blur_shader) {
    if (!config.enabled)
        return;

    // ------------------------------------------------------------------------
    // 1. SSAO Generation Pass
    // ------------------------------------------------------------------------
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glClear(GL_COLOR_BUFFER_BIT); // Clear SSAO texture

    ssao_shader.bind();

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_position); // Depth texture (used for Pos reconstruction)
    glActiveTexture(GL_TEXTURE0 + 1);
    glBindTexture(GL_TEXTURE_2D, g_normal);
    glActiveTexture(GL_TEXTURE0 + 2);
    glBindTexture(GL_TEXTURE_2D, m_noise_texture);

    // Uniforms
    ssao_shader.set_int("u_g_depth", 0);
    ssao_shader.set_int("u_g_normal", 1);
    ssao_shader.set_int("u_tex_noise", 2);

    ssao_shader.set_mat4("u_projection", projection);
    ssao_shader.set_mat4("u_inverse_projection", glm::inverse(projection));

    for (int i = 0; i < config.kernel_size; ++i) {
        ssao_shader.set_vec3("u_samples[" + std::to_string(i) + "]", m_kernel[i]);
    }

    ssao_shader.set_float("u_radius", config.radius);
    ssao_shader.set_float("u_bias", config.bias);
    ssao_shader.set_vec2("u_noise_scale", glm::vec2(m_width / 4.0f, m_height / 4.0f));
    ssao_shader.set_int("u_kernel_size", config.kernel_size);

    render_quad();

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // ------------------------------------------------------------------------
    // 2. Blur Pass
    // ------------------------------------------------------------------------
    glBindFramebuffer(GL_FRAMEBUFFER, m_blur_fbo);
    glClear(GL_COLOR_BUFFER_BIT);

    blur_shader.bind();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_color_texture);
    blur_shader.set_int("u_ssao_input", 0);

    glActiveTexture(GL_TEXTURE0 + 1);
    glBindTexture(GL_TEXTURE_2D, g_position); // Depth texture
    blur_shader.set_int("u_g_depth", 1);

    render_quad();

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void SSAO::render_quad() {
    if (m_quad_vao == 0) {
        float quadVertices[] = {
            // positions        // texCoords
            -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
            1.0f,  1.0f, 0.0f, 1.0f, 1.0f, 1.0f,  -1.0f, 0.0f, 1.0f, 0.0f,
        };
        // Setup plane VAO
        glGenVertexArrays(1, &m_quad_vao);
        glGenBuffers(1, &m_quad_vbo);
        glBindVertexArray(m_quad_vao);
        glBindBuffer(GL_ARRAY_BUFFER, m_quad_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                              (void*)(3 * sizeof(float)));
    }
    glBindVertexArray(m_quad_vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}

} // namespace hz
