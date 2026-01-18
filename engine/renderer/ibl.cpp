#include "ibl.hpp"

#include "engine/core/log.hpp"
#include "opengl/shader.hpp"

#include <fstream>
#include <sstream>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <stb_image.h>

namespace hz {

// Cube vertices for rendering to cubemap faces
static float s_cube_vertices[] = {
    // positions
    -1.0f, 1.0f,  -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f,
    1.0f,  -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f,

    -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  -1.0f,
    -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f, 1.0f,

    1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f, 1.0f,  1.0f,  1.0f,  1.0f,
    1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  -1.0f, 1.0f,  -1.0f, -1.0f,

    -1.0f, -1.0f, 1.0f,  -1.0f, 1.0f,  1.0f,  1.0f,  1.0f,  1.0f,
    1.0f,  1.0f,  1.0f,  1.0f,  -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,

    -1.0f, 1.0f,  -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f,  1.0f,  1.0f,
    1.0f,  1.0f,  1.0f,  -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f,  -1.0f,

    -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f,
    1.0f,  -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f};

// Quad vertices for BRDF LUT
static float s_quad_vertices[] = {
    // positions        // texture coords
    -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
    1.0f,  1.0f, 0.0f, 1.0f, 1.0f, 1.0f,  -1.0f, 0.0f, 1.0f, 0.0f,
};

// Capture projection/view matrices for cubemap rendering
static glm::mat4 s_capture_projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
static glm::mat4 s_capture_views[] = {
    glm::lookAt(glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
    glm::lookAt(glm::vec3(0.0f), glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
    glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
    glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)),
    glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
    glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f))};

static std::string read_shader_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        HZ_ENGINE_ERROR("Failed to open shader file: {}", path);
        return "";
    }
    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

IBL::IBL() = default;

IBL::~IBL() noexcept {
    if (m_capture_fbo)
        glDeleteFramebuffers(1, &m_capture_fbo);
    if (m_capture_rbo)
        glDeleteRenderbuffers(1, &m_capture_rbo);
    if (m_hdr_texture)
        glDeleteTextures(1, &m_hdr_texture);
    if (m_env_cubemap)
        glDeleteTextures(1, &m_env_cubemap);
    if (m_irradiance_map)
        glDeleteTextures(1, &m_irradiance_map);
    if (m_prefilter_map)
        glDeleteTextures(1, &m_prefilter_map);
    if (m_brdf_lut)
        glDeleteTextures(1, &m_brdf_lut);
    if (m_cube_vao)
        glDeleteVertexArrays(1, &m_cube_vao);
    if (m_cube_vbo)
        glDeleteBuffers(1, &m_cube_vbo);
    if (m_quad_vao)
        glDeleteVertexArrays(1, &m_quad_vao);
    if (m_quad_vbo)
        glDeleteBuffers(1, &m_quad_vbo);
}

bool IBL::generate(const std::string& hdr_path, u32 cubemap_size) {
    HZ_ENGINE_INFO("Generating IBL from: {}", hdr_path);

    setup_framebuffer();
    load_hdr_texture(hdr_path);

    if (!m_hdr_texture) {
        HZ_ENGINE_ERROR("Failed to load HDR texture for IBL");
        return false;
    }

    create_environment_cubemap(cubemap_size);
    create_irradiance_map();
    create_prefilter_map(cubemap_size);
    create_brdf_lut();

    m_ready = true;
    HZ_ENGINE_INFO("IBL generation complete!");
    return true;
}

void IBL::bind(u32 irradiance_slot, u32 prefilter_slot, u32 brdf_slot) const {
    if (!m_ready)
        return;

    glActiveTexture(GL_TEXTURE0 + irradiance_slot);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_irradiance_map);

    glActiveTexture(GL_TEXTURE0 + prefilter_slot);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_prefilter_map);

    glActiveTexture(GL_TEXTURE0 + brdf_slot);
    glBindTexture(GL_TEXTURE_2D, m_brdf_lut);
}

void IBL::setup_framebuffer() {
    // Create capture framebuffer
    glGenFramebuffers(1, &m_capture_fbo);
    glGenRenderbuffers(1, &m_capture_rbo);

    // Setup cube VAO
    glGenVertexArrays(1, &m_cube_vao);
    glGenBuffers(1, &m_cube_vbo);

    glBindVertexArray(m_cube_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_cube_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(s_cube_vertices), s_cube_vertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glBindVertexArray(0);

    // Setup quad VAO
    glGenVertexArrays(1, &m_quad_vao);
    glGenBuffers(1, &m_quad_vbo);

    glBindVertexArray(m_quad_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_quad_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(s_quad_vertices), s_quad_vertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glBindVertexArray(0);
}

void IBL::load_hdr_texture(const std::string& path) {
    stbi_set_flip_vertically_on_load(true);
    int width, height, channels;
    float* data = stbi_loadf(path.c_str(), &width, &height, &channels, 0);

    if (!data) {
        HZ_ENGINE_ERROR("Failed to load HDR image: {}", path);
        return;
    }

    glGenTextures(1, &m_hdr_texture);
    glBindTexture(GL_TEXTURE_2D, m_hdr_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_FLOAT, data);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(data);
    HZ_ENGINE_INFO("Loaded HDR texture: {}x{}", width, height);
}

void IBL::create_environment_cubemap(u32 size) {
    // Create environment cubemap
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);

    glGenTextures(1, &m_env_cubemap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_env_cubemap);

    for (u32 i = 0; i < 6; ++i) {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, static_cast<GLsizei>(size),
                     static_cast<GLsizei>(size), 0, GL_RGB, GL_FLOAT, nullptr);
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Load shader
    std::string vert_src = read_shader_file("assets/shaders/equirect_to_cubemap.vert");
    std::string frag_src = read_shader_file("assets/shaders/equirect_to_cubemap.frag");
    gl::Shader equirect_shader(vert_src, frag_src);

    equirect_shader.bind();
    equirect_shader.set_int("u_equirect_map", 0);
    equirect_shader.set_mat4("u_projection", s_capture_projection);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_hdr_texture);

    glViewport(0, 0, static_cast<GLsizei>(size), static_cast<GLsizei>(size));
    glBindFramebuffer(GL_FRAMEBUFFER, m_capture_fbo);

    for (u32 i = 0; i < 6; ++i) {
        equirect_shader.set_mat4("u_view", s_capture_views[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, m_env_cubemap, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        render_cube();
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Generate mipmaps for prefiltering
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_env_cubemap);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

    HZ_ENGINE_INFO("Created environment cubemap: {}x{}", size, size);
}

void IBL::create_irradiance_map() {
    // Higher irradiance resolution reduces blocky diffuse ambient
    const u32 irradiance_size = 64;

    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);

    glGenTextures(1, &m_irradiance_map);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_irradiance_map);

    for (u32 i = 0; i < 6; ++i) {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, irradiance_size,
                     irradiance_size, 0, GL_RGB, GL_FLOAT, nullptr);
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Load shader
    std::string vert_src = read_shader_file("assets/shaders/equirect_to_cubemap.vert");
    std::string frag_src = read_shader_file("assets/shaders/irradiance_convolution.frag");
    gl::Shader irradiance_shader(vert_src, frag_src);

    irradiance_shader.bind();
    irradiance_shader.set_int("u_environment_map", 0);
    irradiance_shader.set_mat4("u_projection", s_capture_projection);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_env_cubemap);

    glViewport(0, 0, irradiance_size, irradiance_size);
    glBindFramebuffer(GL_FRAMEBUFFER, m_capture_fbo);

    for (u32 i = 0; i < 6; ++i) {
        irradiance_shader.set_mat4("u_view", s_capture_views[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, m_irradiance_map, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        render_cube();
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    HZ_ENGINE_INFO("Created irradiance map: {}x{}", irradiance_size, irradiance_size);
}

void IBL::create_prefilter_map(u32 size) {
    // Prefilter resolution heavily affects reflection smoothness.
    // Tie it to the environment cubemap size (passed in), but keep it bounded.
    const u32 prefilter_size = std::max<u32>(128, std::min<u32>(512, size / 4));
    // Use more mips for smoother roughness transitions.
    const u32 max_mip_levels = 6;

    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);

    glGenTextures(1, &m_prefilter_map);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_prefilter_map);

    for (u32 i = 0; i < 6; ++i) {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, prefilter_size,
                     prefilter_size, 0, GL_RGB, GL_FLOAT, nullptr);
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

    // Load shader
    std::string vert_src = read_shader_file("assets/shaders/equirect_to_cubemap.vert");
    std::string frag_src = read_shader_file("assets/shaders/prefilter.frag");
    gl::Shader prefilter_shader(vert_src, frag_src);

    prefilter_shader.bind();
    prefilter_shader.set_int("u_environment_map", 0);
    prefilter_shader.set_mat4("u_projection", s_capture_projection);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_env_cubemap);

    glBindFramebuffer(GL_FRAMEBUFFER, m_capture_fbo);

    for (u32 mip = 0; mip < max_mip_levels; ++mip) {
        // Resize framebuffer according to mip level
        u32 mip_width = static_cast<u32>(prefilter_size * std::pow(0.5, mip));
        u32 mip_height = mip_width;

        glBindRenderbuffer(GL_RENDERBUFFER, m_capture_rbo);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24,
                              static_cast<GLsizei>(mip_width), static_cast<GLsizei>(mip_height));
        glViewport(0, 0, static_cast<GLsizei>(mip_width), static_cast<GLsizei>(mip_height));

        float roughness = static_cast<float>(mip) / static_cast<float>(max_mip_levels - 1);
        prefilter_shader.set_float("u_roughness", roughness);

        for (u32 i = 0; i < 6; ++i) {
            prefilter_shader.set_mat4("u_view", s_capture_views[i]);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                   GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, m_prefilter_map,
                                   static_cast<GLint>(mip));
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            render_cube();
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    HZ_ENGINE_INFO("Created prefilter map: {}x{} with {} mip levels", prefilter_size,
                   prefilter_size, max_mip_levels);
}

void IBL::create_brdf_lut() {
    const u32 lut_size = 512;

    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);

    glGenTextures(1, &m_brdf_lut);
    glBindTexture(GL_TEXTURE_2D, m_brdf_lut);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, lut_size, lut_size, 0, GL_RG, GL_FLOAT, nullptr);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Load shader
    std::string vert_src = read_shader_file("assets/shaders/brdf_lut.vert");
    std::string frag_src = read_shader_file("assets/shaders/brdf_lut.frag");
    gl::Shader brdf_shader(vert_src, frag_src);

    // Render BRDF LUT
    glBindFramebuffer(GL_FRAMEBUFFER, m_capture_fbo);
    glBindRenderbuffer(GL_RENDERBUFFER, m_capture_rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, lut_size, lut_size);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_brdf_lut, 0);

    glViewport(0, 0, lut_size, lut_size);
    brdf_shader.bind();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    render_quad();

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    HZ_ENGINE_INFO("Created BRDF LUT: {}x{}", lut_size, lut_size);
}

void IBL::render_cube() {
    glBindVertexArray(m_cube_vao);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
}

void IBL::render_quad() {
    glBindVertexArray(m_quad_vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}

} // namespace hz
