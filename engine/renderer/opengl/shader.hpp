#pragma once

/**
 * @file shader.hpp
 * @brief RAII OpenGL shader program wrapper
 */

#include "engine/core/types.hpp"
#include "gl_context.hpp"

#include <string>
#include <string_view>
#include <unordered_map>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace hz::gl {

/**
 * @brief RAII wrapper for OpenGL shader program
 */
class Shader {
public:
    /**
     * @brief Create shader from source strings
     */
    Shader(std::string_view vertex_source, std::string_view fragment_source);

    /**
     * @brief Destroy shader program
     */
    ~Shader();

    HZ_NON_COPYABLE(Shader);
    HZ_DEFAULT_MOVABLE(Shader);

    /**
     * @brief Bind this shader for rendering
     */
    void bind() const;

    /**
     * @brief Unbind any shader
     */
    static void unbind();

    /**
     * @brief Get the program ID
     */
    [[nodiscard]] GLuint id() const noexcept { return m_program; }

    /**
     * @brief Check if shader is valid
     */
    [[nodiscard]] bool is_valid() const noexcept { return m_program != 0; }

    // ========================================================================
    // Uniform Setters
    // ========================================================================

    void set_bool(std::string_view name, bool value) const;
    void set_int(std::string_view name, i32 value) const;
    void set_float(std::string_view name, f32 value) const;
    void set_vec2(std::string_view name, const glm::vec2& value) const;
    void set_vec3(std::string_view name, const glm::vec3& value) const;
    void set_vec4(std::string_view name, const glm::vec4& value) const;
    void set_mat3(std::string_view name, const glm::mat3& value) const;
    void set_mat4(std::string_view name, const glm::mat4& value) const;

private:
    [[nodiscard]] GLint get_uniform_location(std::string_view name) const;
    [[nodiscard]] static GLuint compile_shader(GLenum type, std::string_view source);

    GLuint m_program{0};
    mutable std::unordered_map<std::string, GLint> m_uniform_cache;
};

} // namespace hz::gl
