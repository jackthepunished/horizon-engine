#include "rhi_device.hpp"

#include "engine/assets/shader_compiler.hpp"
#include "engine/core/log.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

namespace hz::rhi {

// Safe path-to-string conversion (same as in shader_compiler.cpp)
static std::string path_to_string(const std::filesystem::path& p) {
#ifdef _WIN32
    const auto& ws = p.native();
    if (ws.empty())
        return {};
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, ws.data(), static_cast<int>(ws.size()),
                                          nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<size_t>(size_needed), '\0');
    WideCharToMultiByte(CP_UTF8, 0, ws.data(), static_cast<int>(ws.size()), result.data(),
                        size_needed, nullptr, nullptr);
    return result;
#else
    return p.string();
#endif
}

std::unique_ptr<ShaderModule> Device::create_shader_from_glsl(std::string_view source,
                                                              ShaderStage stage,
                                                              const char* debug_name) {
    ShaderCompileOptions options;
    options.source = source;
    options.stage = stage;
    options.filename = debug_name ? debug_name : "inline_shader";

    ShaderCompileResult result = ShaderCompiler::compile(options);

    if (!result.success) {
        HZ_LOG_ERROR("Failed to compile GLSL shader: {}", result.error_message);
        return nullptr;
    }

    return create_shader_module({std::span<const u8>(result.spirv), stage, "main", debug_name});
}

std::unique_ptr<ShaderModule> Device::create_shader_from_file(const std::filesystem::path& path,
                                                              ShaderStage stage,
                                                              const char* debug_name) {
    // Infer stage from extension if not provided
    if (stage == ShaderStage::None) {
        stage = ShaderCompiler::infer_stage_from_extension(path);
    }

    ShaderCompileResult result = ShaderCompiler::compile_file(path, stage);

    if (!result.success) {
        HZ_LOG_ERROR("Failed to compile shader file '{}': {}", path_to_string(path),
                     result.error_message);
        return nullptr;
    }

    // Use filename as debug name if not provided
    std::string name = debug_name ? debug_name : path_to_string(path.filename());
    return create_shader_module({std::span<const u8>(result.spirv), stage, "main", name.c_str()});
}

} // namespace hz::rhi
