#pragma once

/**
 * @file buffer.hpp
 * @brief RAII OpenGL buffer wrappers (VBO, VAO, EBO)
 */

#include "engine/core/types.hpp"
#include "gl_context.hpp"

#include <span>
#include <vector>

namespace hz::gl {

/**
 * @brief RAII wrapper for OpenGL Vertex Array Object
 */
class VertexArray {
public:
    VertexArray();
    ~VertexArray() noexcept;

    HZ_NON_COPYABLE(VertexArray);

    // Custom move operations to properly handle OpenGL resources
    VertexArray(VertexArray&& other) noexcept : m_vao(other.m_vao) {
        other.m_vao = 0;  // Prevent source from deleting the handle
    }
    VertexArray& operator=(VertexArray&& other) noexcept {
        if (this != &other) {
            if (m_vao != 0) glDeleteVertexArrays(1, &m_vao);
            m_vao = other.m_vao;
            other.m_vao = 0;
        }
        return *this;
    }

    void bind() const;
    static void unbind();

    [[nodiscard]] GLuint id() const noexcept { return m_vao; }

private:
    GLuint m_vao{0};
};

/**
 * @brief Buffer usage hint
 */
enum class BufferUsage : GLenum {
    Static = GL_STATIC_DRAW,
    Dynamic = GL_DYNAMIC_DRAW,
    Stream = GL_STREAM_DRAW
};

/**
 * @brief RAII wrapper for OpenGL Vertex Buffer Object
 */
class VertexBuffer {
public:
    VertexBuffer();
    ~VertexBuffer() noexcept;

    HZ_NON_COPYABLE(VertexBuffer);

    // Custom move operations to properly handle OpenGL resources
    VertexBuffer(VertexBuffer&& other) noexcept : m_vbo(other.m_vbo), m_size(other.m_size) {
        other.m_vbo = 0;
        other.m_size = 0;
    }
    VertexBuffer& operator=(VertexBuffer&& other) noexcept {
        if (this != &other) {
            if (m_vbo != 0) glDeleteBuffers(1, &m_vbo);
            m_vbo = other.m_vbo;
            m_size = other.m_size;
            other.m_vbo = 0;
            other.m_size = 0;
        }
        return *this;
    }

    void bind() const;
    static void unbind();

    /**
     * @brief Upload data to buffer
     */
    void set_data(std::span<const std::byte> data, BufferUsage usage = BufferUsage::Static);

    template <typename T>
    void set_data(std::span<const T> data, BufferUsage usage = BufferUsage::Static) {
        set_data(std::as_bytes(data), usage);
    }

    /**
     * @brief Update subset of buffer data
     */
    void set_sub_data(usize offset, std::span<const std::byte> data);

    [[nodiscard]] GLuint id() const noexcept { return m_vbo; }
    [[nodiscard]] usize size() const noexcept { return m_size; }

private:
    GLuint m_vbo{0};
    usize m_size{0};
};

/**
 * @brief RAII wrapper for OpenGL Element Buffer Object
 */
class IndexBuffer {
public:
    IndexBuffer();
    ~IndexBuffer() noexcept;

    HZ_NON_COPYABLE(IndexBuffer);

    // Custom move operations to properly handle OpenGL resources
    IndexBuffer(IndexBuffer&& other) noexcept : m_ebo(other.m_ebo), m_count(other.m_count) {
        other.m_ebo = 0;
        other.m_count = 0;
    }
    IndexBuffer& operator=(IndexBuffer&& other) noexcept {
        if (this != &other) {
            if (m_ebo != 0) glDeleteBuffers(1, &m_ebo);
            m_ebo = other.m_ebo;
            m_count = other.m_count;
            other.m_ebo = 0;
            other.m_count = 0;
        }
        return *this;
    }

    void bind() const;
    static void unbind();

    /**
     * @brief Upload index data
     */
    void set_data(std::span<const u32> indices, BufferUsage usage = BufferUsage::Static);

    [[nodiscard]] GLuint id() const noexcept { return m_ebo; }
    [[nodiscard]] u32 count() const noexcept { return m_count; }

private:
    GLuint m_ebo{0};
    u32 m_count{0};
};

// ============================================================================
// Vertex Attribute Layout
// ============================================================================

/**
 * @brief Describes a vertex attribute for layout specification
 */
struct VertexAttribute {
    u32 index;
    i32 size;    // 1, 2, 3, or 4 components
    GLenum type; // GL_FLOAT, GL_INT, etc.
    bool normalized;
    usize stride;
    usize offset;
};

/**
 * @brief Configure vertex attribute pointer
 */
inline void set_vertex_attrib(const VertexAttribute& attr) {
    glEnableVertexAttribArray(attr.index);
    glVertexAttribPointer(attr.index, attr.size, attr.type, attr.normalized ? GL_TRUE : GL_FALSE,
                          static_cast<GLsizei>(attr.stride),
                          reinterpret_cast<const void*>(attr.offset));
}

/**
 * @brief Describes an integer vertex attribute (for bone IDs, etc.)
 */
struct IntVertexAttribute {
    u32 index;
    i32 size;    // 1, 2, 3, or 4 components
    GLenum type; // GL_INT, GL_UNSIGNED_INT, etc.
    usize stride;
    usize offset;
};

/**
 * @brief Configure integer vertex attribute pointer
 *
 * Note: Using glVertexAttribPointer since glVertexAttribIPointer may not be
 * available in all GLAD configurations. Shader will receive as float.
 */
inline void set_vertex_attrib_int(const IntVertexAttribute& attr) {
    glEnableVertexAttribArray(attr.index);
    // Correctly use glVertexAttribIPointer for integer attributes so they are not converted to
    // floats.
    glVertexAttribIPointer(attr.index, attr.size, attr.type, static_cast<GLsizei>(attr.stride),
                           reinterpret_cast<const void*>(attr.offset));
}

} // namespace hz::gl
