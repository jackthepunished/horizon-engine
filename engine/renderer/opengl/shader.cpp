#include "shader.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <unordered_set>
#include <vector>

namespace hz::gl {

Shader::Shader(std::string_view vertex_source, std::string_view fragment_source) {
    // Determine shader directory (hardcoded for now as we don't pass path here)
    // In a real engine, we'd pass the file path. For now, assume relative to assets/shaders
    std::filesystem::path shader_dir = "assets/shaders";

    std::stringstream processed_vert_stream;
    std::unordered_set<std::string> included_files_vert;
    if (!process_shader_source(vertex_source, processed_vert_stream, shader_dir,
                               included_files_vert)) {
        throw std::runtime_error("Failed to preprocess vertex shader");
    }

    std::stringstream processed_frag_stream;
    std::unordered_set<std::string> included_files_frag;
    if (!process_shader_source(fragment_source, processed_frag_stream, shader_dir,
                               included_files_frag)) {
        throw std::runtime_error("Failed to preprocess fragment shader");
    }

    GLuint vertex = compile_shader(GL_VERTEX_SHADER, processed_vert_stream.str());
    if (vertex == 0) {
        throw std::runtime_error("Failed to compile vertex shader");
    }

    GLuint fragment = compile_shader(GL_FRAGMENT_SHADER, processed_frag_stream.str());
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

// ----------------------------------------------------------------------------
// Shader Preprocessor
// ----------------------------------------------------------------------------
bool Shader::process_shader_source(std::string_view source, std::stringstream& final_stream,
                                   const std::filesystem::path& shader_dir,
                                   std::unordered_set<std::string>& included_files) {

    std::stringstream source_stream;
    source_stream << source;
    std::string line;

    while (std::getline(source_stream, line)) {
        if (line.find("#include") != std::string::npos) {
            // Found include directive
            size_t start = line.find("\"");
            size_t end = line.rfind("\"");

            if (start != std::string::npos && end != std::string::npos && end > start) {
                std::string include_path_str = line.substr(start + 1, end - start - 1);
                std::filesystem::path include_path = shader_dir / include_path_str;

                // Check if file exists, if not try relative to assets/shaders root (optional
                // fallback) For now, strict relative to current file

                std::error_code ec;
                std::filesystem::path abs_path =
                    std::filesystem::weakly_canonical(include_path, ec);

                if (ec) {
                    HZ_ENGINE_ERROR("Shader Preprocessor: Failed to resolve path: {}",
                                    include_path.string());
                    return false;
                }

                std::string abs_path_str = abs_path.string();

                // Pragma once check
                if (included_files.find(abs_path_str) != included_files.end()) {
                    continue; // Already included
                }

                // Load included file
                std::ifstream file(include_path);
                if (file.is_open()) {
                    std::stringstream buffer;
                    buffer << file.rdbuf();
                    included_files.insert(abs_path_str);

                    // Recursive processing
                    if (!process_shader_source(buffer.str(), final_stream, abs_path.parent_path(),
                                               included_files)) {
                        return false;
                    }
                    final_stream << "\n"; // Ensure separation
                } else {
                    HZ_ENGINE_ERROR("Shader Preprocessor: Failed to open include file: {}",
                                    include_path.string());
                    return false;
                }
            } else {
                // Invalid syntax, just print line
                HZ_ENGINE_WARN("Shader Preprocessor: Invalid #include syntax: {}", line);
                final_stream << line << "\n";
            }
        } else {
            final_stream << line << "\n";
        }
    }
    return true;
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

void Shader::set_mat4_array(std::string_view name, const glm::mat4* values, u32 count) const {
    glUniformMatrix4fv(get_uniform_location(name), static_cast<GLsizei>(count), GL_FALSE,
                       glm::value_ptr(values[0]));
}

void Shader::bind_uniform_block(std::string_view name, u32 binding_point) const {
    std::string name_str(name);
    GLuint block_index = glGetUniformBlockIndex(m_program, name_str.c_str());
    if (block_index != GL_INVALID_INDEX) {
        glUniformBlockBinding(m_program, block_index, binding_point);
    } else {
        HZ_ENGINE_WARN("Shader {}: Uniform block '{}' not found or active", m_program, name);
    }
}

} // namespace hz::gl
