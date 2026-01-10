#pragma once

/**
 * @file framebuffer.hpp
 * @brief OpenGL Framebuffer wrapper
 */

#include "engine/core/types.hpp"
#include "gl_context.hpp"

#include <glm/glm.hpp>

namespace hz::gl {

struct FramebufferConfig {
    u32 width{1024};
    u32 height{1024};
    bool depth_only{false};     // For shadow maps
    bool hdr{false};            // Use Floating Point Color Attachment
    bool depth_sampling{false}; // Create Depth Texture instead of RBO
};

class Framebuffer {
public:
    explicit Framebuffer(const FramebufferConfig& config);
    ~Framebuffer();

    HZ_NON_COPYABLE(Framebuffer);
    HZ_DEFAULT_MOVABLE(Framebuffer);

    void bind() const;
    void unbind() const;

    [[nodiscard]] GLuint id() const { return m_fbo; }
    [[nodiscard]] GLuint get_texture_id() const { return m_texture_id; }
    [[nodiscard]] GLuint get_depth_texture_id() const { return m_depth_texture_id; }
    [[nodiscard]] const FramebufferConfig& config() const { return m_config; }

private:
    void invalidate();

    FramebufferConfig m_config;
    GLuint m_fbo{0};
    GLuint m_texture_id{0};       // Color attachment (or Depth if depth_only)
    GLuint m_depth_texture_id{0}; // Separate depth texture (if depth_sampling)
    GLuint m_rbo{0};              // Renderbuffer (if needed)
};

} // namespace hz::gl
