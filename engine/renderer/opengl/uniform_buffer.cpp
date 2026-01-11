#include "uniform_buffer.hpp"

namespace hz::gl {

UniformBuffer::UniformBuffer(usize size, u32 binding_point)
    : m_size(size), m_binding_point(binding_point) {
    glGenBuffers(1, &m_ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, m_ubo);
    glBufferData(GL_UNIFORM_BUFFER, static_cast<GLsizeiptr>(size), nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, binding_point, m_ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

UniformBuffer::~UniformBuffer() noexcept {
    if (m_ubo) {
        glDeleteBuffers(1, &m_ubo);
    }
}

void UniformBuffer::bind() const {
    glBindBuffer(GL_UNIFORM_BUFFER, m_ubo);
}

void UniformBuffer::unbind() {
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void UniformBuffer::set_data(const void* data, usize size, usize offset) {
    glBindBuffer(GL_UNIFORM_BUFFER, m_ubo);
    glBufferSubData(GL_UNIFORM_BUFFER, static_cast<GLintptr>(offset), static_cast<GLsizeiptr>(size),
                    data);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

} // namespace hz::gl
