#include "framebuffer.hpp"

#include "engine/core/log.hpp"

namespace hz::gl {

Framebuffer::Framebuffer(const FramebufferConfig& config) : m_config(config) {
    invalidate();
}

Framebuffer::~Framebuffer() {
    if (m_fbo) {
        glDeleteFramebuffers(1, &m_fbo);
        glDeleteTextures(1, &m_texture_id);
        if (m_rbo)
            glDeleteRenderbuffers(1, &m_rbo); // RBO might be 0 if depth_only
    }
}

void Framebuffer::invalidate() {
    if (m_fbo) {
        glDeleteFramebuffers(1, &m_fbo);
        glDeleteTextures(1, &m_texture_id);
        if (m_depth_texture_id)
            glDeleteTextures(1, &m_depth_texture_id);
        if (m_rbo)
            glDeleteRenderbuffers(1, &m_rbo);
    }

    glGenFramebuffers(1, &m_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

    if (m_config.depth_only) {
        // Depth Attachment (for Shadow Map)
        glGenTextures(1, &m_texture_id);
        glBindTexture(GL_TEXTURE_2D, m_texture_id);

        // Use GL_DEPTH_COMPONENT32F for precision and float compatibility
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, static_cast<GLsizei>(m_config.width),
                     static_cast<GLsizei>(m_config.height), 0, GL_DEPTH_COMPONENT, GL_FLOAT,
                     nullptr);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        float border_color[] = {1.0f, 1.0f, 1.0f, 1.0f};
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border_color);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_texture_id, 0);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
    } else {
        // Color Attachment (Standard or HDR FBO)
        glGenTextures(1, &m_texture_id);
        glBindTexture(GL_TEXTURE_2D, m_texture_id);

        GLenum internal_format = m_config.hdr ? GL_RGBA16F : GL_RGB;
        GLenum format = m_config.hdr ? GL_RGBA : GL_RGB;
        GLenum type = m_config.hdr ? GL_FLOAT : GL_UNSIGNED_BYTE;

        glTexImage2D(GL_TEXTURE_2D, 0, internal_format, static_cast<GLsizei>(m_config.width),
                     static_cast<GLsizei>(m_config.height), 0, format, type, nullptr);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_texture_id,
                               0);

        if (m_config.depth_sampling) {
            // Create Depth Texture for sampling
            glGenTextures(1, &m_depth_texture_id);
            glBindTexture(GL_TEXTURE_2D, m_depth_texture_id);
            glTexImage2D(
                GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, static_cast<GLsizei>(m_config.width),
                static_cast<GLsizei>(m_config.height), 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
                                   m_depth_texture_id, 0);
        } else {
            // Create Renderbuffer for Depth/Stencil since we are not sampling depth
            glGenRenderbuffers(1, &m_rbo);
            glBindRenderbuffer(GL_RENDERBUFFER, m_rbo);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8,
                                  static_cast<GLsizei>(m_config.width),
                                  static_cast<GLsizei>(m_config.height));
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                                      m_rbo);
        }
    }

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        HZ_ENGINE_ERROR("Framebuffer is incomplete! Status: 0x{0:x}", status);
        if (status == GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT)
            HZ_ENGINE_ERROR("Cause: Incomplete Attachment");
        if (status == GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT)
            HZ_ENGINE_ERROR("Cause: Missing Attachment");
        if (status == GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER)
            HZ_ENGINE_ERROR("Cause: Incomplete Draw Buffer");
        if (status == GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER)
            HZ_ENGINE_ERROR("Cause: Incomplete Read Buffer");
        if (status == GL_FRAMEBUFFER_UNSUPPORTED)
            HZ_ENGINE_ERROR("Cause: Unsupported");
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Framebuffer::bind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glViewport(0, 0, static_cast<GLsizei>(m_config.width), static_cast<GLsizei>(m_config.height));
}

void Framebuffer::unbind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

} // namespace hz::gl
