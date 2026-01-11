#pragma once

/**
 * @file uniform_buffer.hpp
 * @brief RAII wrapper for OpenGL Uniform Buffer Object (UBO)
 */

#include "engine/core/types.hpp"
#include "gl_context.hpp"

namespace hz::gl {

/**
 * @brief RAII wrapper for OpenGL Uniform Buffer Object
 */
class UniformBuffer {
public:
    /**
     * @brief Create a new Uniform Buffer Object
     * @param size Size of the buffer in bytes
     * @param binding_point Binding point index (e.g., 0 for Camera, 1 for Scene)
     */
    UniformBuffer(usize size, u32 binding_point);
    ~UniformBuffer() noexcept;

    HZ_NON_COPYABLE(UniformBuffer);
    HZ_DEFAULT_MOVABLE(UniformBuffer);

    void bind() const;
    static void unbind();

    /**
     * @brief Upload data to buffer
     * @param data Pointer to data
     * @param size Size of data in bytes
     * @param offset Offset in buffer in bytes
     */
    void set_data(const void* data, usize size, usize offset = 0);

    /**
     * @brief Upload typed data to buffer
     */
    template <typename T>
    void set_data(const T& data, usize offset = 0) {
        set_data(&data, sizeof(T), offset);
    }

    [[nodiscard]] GLuint id() const noexcept { return m_ubo; }
    [[nodiscard]] usize size() const noexcept { return m_size; }
    [[nodiscard]] u32 binding_point() const noexcept { return m_binding_point; }

private:
    GLuint m_ubo{0};
    usize m_size{0};
    u32 m_binding_point{0};
};

} // namespace hz::gl
