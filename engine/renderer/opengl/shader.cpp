#include "shader.hpp"

#include <stdexcept>
#include <vector>

namespace hz::gl {

Shader::Shader(std::string_view vertex_source, std::string_view fragment_source) {
    GLuint vertex = compile_shader(GL_VERTEX_SHADER, vertex_source);
    if (vertex == 0) {
        throw std::runtime_error("Failed to compile vertex shader");
    }

    GLuint fragment = compile_shader(GL_FRAGMENT_SHADER, fragment_source);
    if (fragment == 0) {
        glDeleteShader(vertex);
        throw std::runtime_error("Failed to compile fragment shader");
    }

    m_program = glCreateProgram();
    glAttachShader(m_program, vertex);
    glAttachShader(m_program, fragment);
    glLinkProgram(m_program);

    // Check link status
    GLint success;
    glGetProgramiv(m_program, GL_LINK_STATUS, &success);
    if (!success) {
        char info_log[512];
        glGetProgramInfoLog(m_program, 512, nullptr, info_log);
        HZ_ENGINE_ERROR("Shader link error: {}", info_log);

        glDeleteShader(vertex);
        glDeleteShader(fragment);
        glDeleteProgram(m_program);
        m_program = 0;
        throw std::runtime_error("Failed to link shader program");
    }

    // Clean up individual shaders
    glDeleteShader(vertex);
    glDeleteShader(fragment);

    HZ_ENGINE_TRACE("Shader program {} created", m_program);
}

Shader::~Shader() {
    if (m_program != 0) {
        glDeleteProgram(m_program);
        HZ_ENGINE_TRACE("Shader program {} destroyed", m_program);
    }
}

void Shader::bind() const {
    glUseProgram(m_program);
}

void Shader::unbind() {
    glUseProgram(0);
}

GLint Shader::get_uniform_location(std::string_view name) const {
    std::string name_str(name);
    auto it = m_uniform_cache.find(name_str);
    if (it != m_uniform_cache.end()) {
        return it->second;
    }

    GLint location = glGetUniformLocation(m_program, name_str.c_str());
    m_uniform_cache[name_str] = location;
    return location;
}

GLuint Shader::compile_shader(GLenum type, std::string_view source) {
    GLuint shader = glCreateShader(type);
    const char* src = source.data();
    GLint length = static_cast<GLint>(source.size());
    glShaderSource(shader, 1, &src, &length);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char info_log[512];
        glGetShaderInfoLog(shader, 512, nullptr, info_log);
        HZ_ENGINE_ERROR("Shader compile error: {}", info_log);
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

// Uniform setters
void Shader::set_bool(std::string_view name, bool value) const {
    glUniform1i(get_uniform_location(name), value ? 1 : 0);
}

void Shader::set_int(std::string_view name, i32 value) const {
    glUniform1i(get_uniform_location(name), value);
}

void Shader::set_float(std::string_view name, f32 value) const {
    glUniform1f(get_uniform_location(name), value);
}

void Shader::set_vec2(std::string_view name, const glm::vec2& value) const {
    glUniform2fv(get_uniform_location(name), 1, glm::value_ptr(value));
}

void Shader::set_vec3(std::string_view name, const glm::vec3& value) const {
    glUniform3fv(get_uniform_location(name), 1, glm::value_ptr(value));
}

void Shader::set_vec4(std::string_view name, const glm::vec4& value) const {
    glUniform4fv(get_uniform_location(name), 1, glm::value_ptr(value));
}

void Shader::set_mat3(std::string_view name, const glm::mat3& value) const {
    glUniformMatrix3fv(get_uniform_location(name), 1, GL_FALSE, glm::value_ptr(value));
}

void Shader::set_mat4(std::string_view name, const glm::mat4& value) const {
    glUniformMatrix4fv(get_uniform_location(name), 1, GL_FALSE, glm::value_ptr(value));
}

} // namespace hz::gl
