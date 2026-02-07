#pragma once

/**
 * @file shader_compiler.hpp
 * @brief Runtime GLSL to SPIR-V shader compilation
 *
 * Compiles GLSL shaders to SPIR-V bytecode at runtime using shaderc.
 * Supports #include directives and shader macro definitions.
 */

#include "engine/core/types.hpp"
#include "engine/rhi/rhi_types.hpp"

#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace hz {

/**
 * @brief Options for shader compilation
 */
struct ShaderCompileOptions {
    std::string_view source;   ///< GLSL source code
    std::string_view filename; ///< Filename for error messages (can be empty)
    rhi::ShaderStage stage{rhi::ShaderStage::None};

    /// Macro definitions: {name, value} pairs
    std::vector<std::pair<std::string, std::string>> defines;

    /// Include search paths (in addition to shader's directory)
    std::vector<std::filesystem::path> include_paths;

    /// Optimization level: 0 = none, 1 = size, 2 = performance (default)
    u32 optimization_level{2};

    /// Generate debug info
    bool debug_info{false};
};

/**
 * @brief Result of shader compilation
 */
struct ShaderCompileResult {
    std::vector<u8> spirv;     ///< Compiled SPIR-V bytecode
    std::string error_message; ///< Error/warning messages
    bool success{false};       ///< True if compilation succeeded
};

/**
 * @brief Static shader compiler utility
 *
 * Thread-safe: each call creates its own compiler instance.
 */
class ShaderCompiler {
public:
    /**
     * @brief Compile GLSL source to SPIR-V
     * @param options Compilation options
     * @return Compilation result with SPIR-V or error message
     */
    [[nodiscard]] static ShaderCompileResult compile(const ShaderCompileOptions& options);

    /**
     * @brief Compile a shader file to SPIR-V
     * @param path Path to the GLSL shader file
     * @param stage Shader stage (if None, inferred from extension)
     * @return Compilation result with SPIR-V or error message
     */
    [[nodiscard]] static ShaderCompileResult
    compile_file(const std::filesystem::path& path,
                 rhi::ShaderStage stage = rhi::ShaderStage::None);

    /**
     * @brief Infer shader stage from file extension
     * @param path Shader file path
     * @return Inferred shader stage, or None if unknown
     */
    [[nodiscard]] static rhi::ShaderStage
    infer_stage_from_extension(const std::filesystem::path& path);
};

} // namespace hz
