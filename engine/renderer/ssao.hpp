#pragma once

#include "engine/core/types.hpp"
#include "engine/renderer/opengl/shader.hpp"

#include <random>
#include <vector>

#include <glm/glm.hpp>

namespace hz {

struct SSAOConfig {
    bool enabled{true};
    int kernel_size{64};
    float radius{0.5f};
    float bias{0.025f};
    float power{2.0f};            // Contrast power
    float resolution_scale{0.5f}; // Render at half resolution for performance
};

class SSAO {
public:
    SSAO() = default;
    ~SSAO();

    void create(u32 width, u32 height, const SSAOConfig& config = {});
    void destroy();

    void resize(u32 width, u32 height);

    // Returns the texture ID of the blurred SSAO result
    u32 get_output_texture() const { return m_blur_texture; }

    // Main render function
    void render(u32 g_position, u32 g_normal, const glm::mat4& projection,
                const gl::Shader& ssao_shader, const gl::Shader& blur_shader);

    SSAOConfig config;

private:
    void generate_kernel();
    void generate_noise();
    void init_framebuffers(u32 width, u32 height);
    void render_quad(); // Helper for quad rendering

    u32 m_fbo{0};
    u32 m_color_texture{0}; // Raw SSAO output

    u32 m_blur_fbo{0};
    u32 m_blur_texture{0}; // Blurred SSAO output

    u32 m_noise_texture{0};
    std::vector<glm::vec3> m_kernel;
    std::vector<glm::vec3> m_noise;

    u32 m_quad_vao{0};
    u32 m_quad_vbo{0};

    u32 m_width{0};
    u32 m_height{0};

    // Shader programs (loaded from AssetManager or Renderer)
    // For now we assume they are managed externally or identifiers stored here
    // Note: In this architecture, we might want to store Shader instance or ID.
    // We will use standard shader loading in implementation.
};

} // namespace hz
