#include "renderer.hpp"

#include "engine/core/log.hpp"
#include "opengl/gl_context.hpp"

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
}

Renderer::~Renderer() {
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

} // namespace hz
