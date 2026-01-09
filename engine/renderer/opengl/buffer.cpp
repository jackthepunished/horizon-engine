#include "buffer.hpp"

namespace hz::gl {

// ============================================================================
// VertexArray Implementation
// ============================================================================

VertexArray::VertexArray() {
    glGenVertexArrays(1, &m_vao);
}

VertexArray::~VertexArray() {
    if (m_vao != 0) {
        glDeleteVertexArrays(1, &m_vao);
    }
}

void VertexArray::bind() const {
    glBindVertexArray(m_vao);
}

void VertexArray::unbind() {
    glBindVertexArray(0);
}

// ============================================================================
// VertexBuffer Implementation
// ============================================================================

VertexBuffer::VertexBuffer() {
    glGenBuffers(1, &m_vbo);
}

VertexBuffer::~VertexBuffer() {
    if (m_vbo != 0) {
        glDeleteBuffers(1, &m_vbo);
    }
}

void VertexBuffer::bind() const {
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
}

void VertexBuffer::unbind() {
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void VertexBuffer::set_data(std::span<const std::byte> data, BufferUsage usage) {
    bind();
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(data.size()), data.data(),
                 static_cast<GLenum>(usage));
    m_size = data.size();
}

void VertexBuffer::set_sub_data(usize offset, std::span<const std::byte> data) {
    bind();
    glBufferSubData(GL_ARRAY_BUFFER, static_cast<GLintptr>(offset),
                    static_cast<GLsizeiptr>(data.size()), data.data());
}

// ============================================================================
// IndexBuffer Implementation
// ============================================================================

IndexBuffer::IndexBuffer() {
    glGenBuffers(1, &m_ebo);
}

IndexBuffer::~IndexBuffer() {
    if (m_ebo != 0) {
        glDeleteBuffers(1, &m_ebo);
    }
}

void IndexBuffer::bind() const {
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
}

void IndexBuffer::unbind() {
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void IndexBuffer::set_data(std::span<const u32> indices, BufferUsage usage) {
    bind();
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(indices.size() * sizeof(u32)),
                 indices.data(), static_cast<GLenum>(usage));
    m_count = static_cast<u32>(indices.size());
}

} // namespace hz::gl
