#pragma once

/**
 * @file ibl.hpp
 * @brief Image-Based Lighting for PBR
 *
 * Generates irradiance map (diffuse IBL), prefiltered environment map
 * (specular IBL), and BRDF LUT from an HDR environment map.
 */

#include "engine/core/types.hpp"
#include "opengl/gl_context.hpp"

#include <string>

namespace hz {

namespace gl {
class Shader;
}

/**
 * @brief IBL (Image-Based Lighting) processor
 *
 * Takes an equirectangular HDR environment map and generates:
 * - Environment cubemap (from equirectangular)
 * - Irradiance map (diffuse IBL, 32x32 per face)
 * - Prefiltered environment map (specular IBL, with mipmaps)
 * - BRDF LUT (2D texture, 512x512)
 */
class IBL {
public:
    IBL();
    ~IBL();

    HZ_NON_COPYABLE(IBL);
    HZ_DEFAULT_MOVABLE(IBL);

    /**
     * @brief Generate all IBL textures from an HDR environment map
     * @param hdr_path Path to equirectangular HDR image
     * @param cubemap_size Size of environment cubemap (default 512)
     * @return true if successful
     */
    bool generate(const std::string& hdr_path, u32 cubemap_size = 512);

    /**
     * @brief Bind IBL textures for rendering
     * @param irradiance_slot Texture slot for irradiance map
     * @param prefilter_slot Texture slot for prefiltered map
     * @param brdf_slot Texture slot for BRDF LUT
     */
    void bind(u32 irradiance_slot = 7, u32 prefilter_slot = 8, u32 brdf_slot = 9) const;

    /**
     * @brief Get environment cubemap (for skybox rendering)
     */
    [[nodiscard]] GLuint environment_map() const { return m_env_cubemap; }

    /**
     * @brief Get irradiance map
     */
    [[nodiscard]] GLuint irradiance_map() const { return m_irradiance_map; }

    /**
     * @brief Get prefiltered environment map
     */
    [[nodiscard]] GLuint prefilter_map() const { return m_prefilter_map; }

    /**
     * @brief Get BRDF LUT
     */
    [[nodiscard]] GLuint brdf_lut() const { return m_brdf_lut; }

    /**
     * @brief Check if IBL is ready
     */
    [[nodiscard]] bool is_ready() const { return m_ready; }

private:
    void setup_framebuffer();
    void load_hdr_texture(const std::string& path);
    void create_environment_cubemap(u32 size);
    void create_irradiance_map();
    void create_prefilter_map(u32 size);
    void create_brdf_lut();
    void render_cube();
    void render_quad();

    // Capture framebuffer
    GLuint m_capture_fbo{0};
    GLuint m_capture_rbo{0};

    // Source HDR texture (equirectangular)
    GLuint m_hdr_texture{0};

    // Generated IBL textures
    GLuint m_env_cubemap{0};    // Environment cubemap (from HDR)
    GLuint m_irradiance_map{0}; // Diffuse IBL (32x32 per face)
    GLuint m_prefilter_map{0};  // Specular IBL (with mipmaps)
    GLuint m_brdf_lut{0};       // BRDF integration LUT (512x512)

    // Render helpers
    GLuint m_cube_vao{0};
    GLuint m_cube_vbo{0};
    GLuint m_quad_vao{0};
    GLuint m_quad_vbo{0};

    bool m_ready{false};
};

} // namespace hz
