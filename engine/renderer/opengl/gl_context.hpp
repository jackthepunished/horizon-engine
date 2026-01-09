#pragma once

/**
 * @file gl_context.hpp
 * @brief OpenGL context and error handling utilities
 */

#include "engine/core/log.hpp"
#include "engine/core/types.hpp"
#include "engine/vendor/glad/glad.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace hz::gl {

// ============================================================================
// Error Handling
// ============================================================================

/**
 * @brief Check for OpenGL errors (debug builds only)
 */
inline void check_error(std::string_view context = "") {
#ifdef HZ_DEBUG
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        const char* error_str = "Unknown";
        switch (error) {
        case GL_INVALID_ENUM:
            error_str = "GL_INVALID_ENUM";
            break;
        case GL_INVALID_VALUE:
            error_str = "GL_INVALID_VALUE";
            break;
        case GL_INVALID_OPERATION:
            error_str = "GL_INVALID_OPERATION";
            break;
        case GL_OUT_OF_MEMORY:
            error_str = "GL_OUT_OF_MEMORY";
            break;
        case GL_INVALID_FRAMEBUFFER_OPERATION:
            error_str = "GL_INVALID_FRAMEBUFFER_OPERATION";
            break;
        }
        HZ_ENGINE_ERROR("OpenGL error: {} ({})", error_str, context);
    }
#else
    HZ_UNUSED(context);
#endif
}

/**
 * @brief Debug callback for OpenGL debug output (if available)
 */
inline void GLAPIENTRY debug_callback(GLenum source, GLenum type, GLuint id, GLenum severity,
                                      GLsizei /*length*/, const GLchar* message,
                                      const void* /*userParam*/) {

    // Ignore non-significant error/warning codes
    if (id == 131169 || id == 131185 || id == 131218 || id == 131204) {
        return;
    }

    const char* source_str = "Unknown";
    switch (source) {
    case GL_DEBUG_SOURCE_API:
        source_str = "API";
        break;
    case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
        source_str = "Window System";
        break;
    case GL_DEBUG_SOURCE_SHADER_COMPILER:
        source_str = "Shader Compiler";
        break;
    case GL_DEBUG_SOURCE_THIRD_PARTY:
        source_str = "Third Party";
        break;
    case GL_DEBUG_SOURCE_APPLICATION:
        source_str = "Application";
        break;
    case GL_DEBUG_SOURCE_OTHER:
        source_str = "Other";
        break;
    }

    const char* type_str = "Unknown";
    switch (type) {
    case GL_DEBUG_TYPE_ERROR:
        type_str = "Error";
        break;
    case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
        type_str = "Deprecated";
        break;
    case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
        type_str = "Undefined Behavior";
        break;
    case GL_DEBUG_TYPE_PORTABILITY:
        type_str = "Portability";
        break;
    case GL_DEBUG_TYPE_PERFORMANCE:
        type_str = "Performance";
        break;
    case GL_DEBUG_TYPE_MARKER:
        type_str = "Marker";
        break;
    case GL_DEBUG_TYPE_OTHER:
        type_str = "Other";
        break;
    }

    switch (severity) {
    case GL_DEBUG_SEVERITY_HIGH:
        HZ_ENGINE_ERROR("[GL {}:{}] {}", source_str, type_str, message);
        break;
    case GL_DEBUG_SEVERITY_MEDIUM:
        HZ_ENGINE_WARN("[GL {}:{}] {}", source_str, type_str, message);
        break;
    case GL_DEBUG_SEVERITY_LOW:
    case GL_DEBUG_SEVERITY_NOTIFICATION:
        HZ_ENGINE_TRACE("[GL {}:{}] {}", source_str, type_str, message);
        break;
    }
}

/**
 * @brief Initialize OpenGL context after GLFW window creation
 * @return true if initialization successful
 */
inline bool init_context() {
    if (!gladLoadGLLoader(reinterpret_cast<void* (*)(const char*)>(glfwGetProcAddress))) {
        HZ_ENGINE_ERROR("Failed to initialize GLAD");
        return false;
    }

    HZ_ENGINE_INFO("OpenGL {}.{} initialized", GLVersion.major, GLVersion.minor);
    HZ_ENGINE_INFO("Vendor: {}", reinterpret_cast<const char*>(glGetString(GL_VENDOR)));
    HZ_ENGINE_INFO("Renderer: {}", reinterpret_cast<const char*>(glGetString(GL_RENDERER)));

    // Enable debug output if available (OpenGL 4.3+)
#ifdef HZ_DEBUG
    if (glDebugMessageCallback) {
        GLint flags;
        glGetIntegerv(GL_CONTEXT_FLAGS, &flags);
        if (flags & GL_CONTEXT_FLAG_DEBUG_BIT) {
            glEnable(GL_DEBUG_OUTPUT);
            glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
            glDebugMessageCallback(debug_callback, nullptr);
            glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
            HZ_ENGINE_TRACE("OpenGL debug output enabled");
        }
    }
#endif

    return true;
}

} // namespace hz::gl
