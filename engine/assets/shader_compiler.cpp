#include "shader_compiler.hpp"

#include "engine/core/log.hpp"

#include <fstream>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#endif

#ifdef HZ_VULKAN_BACKEND
#include <shaderc/shaderc.hpp>
#endif

namespace hz {

// Safe path-to-string conversion that avoids MinGW's broken narrow locale
// conversion (which throws "Illegal byte sequence" on Turkish Windows etc.)
static std::string path_to_string(const std::filesystem::path& p) {
#ifdef _WIN32
    // On Windows, go through wide string -> narrow via WideCharToMultiByte CP_UTF8
    const auto& ws = p.native(); // returns const wstring& on Windows
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

namespace {

#ifdef HZ_VULKAN_BACKEND

/**
 * @brief Custom includer for shaderc that resolves #include directives
 */
class ShaderIncluder : public shaderc::CompileOptions::IncluderInterface {
public:
    explicit ShaderIncluder(std::vector<std::filesystem::path> include_paths)
        : m_include_paths(std::move(include_paths)) {}

    shaderc_include_result* GetInclude(const char* requested_source, shaderc_include_type type,
                                       const char* requesting_source,
                                       [[maybe_unused]] size_t include_depth) override {
        auto* result = new shaderc_include_result{};

        std::filesystem::path resolved_path;

        // For relative includes, first try relative to the requesting file
        if (type == shaderc_include_type_relative) {
            std::filesystem::path requesting_dir =
                std::filesystem::path(requesting_source).parent_path();
            std::filesystem::path candidate = requesting_dir / requested_source;
            if (std::filesystem::exists(candidate)) {
                resolved_path = candidate;
            }
        }

        // Search include paths
        if (resolved_path.empty()) {
            for (const auto& include_path : m_include_paths) {
                std::filesystem::path candidate = include_path / requested_source;
                if (std::filesystem::exists(candidate)) {
                    resolved_path = candidate;
                    break;
                }
            }
        }

        if (resolved_path.empty()) {
            std::string error_msg = "Could not find include file: " + std::string(requested_source);
            auto* error_data = new std::string(std::move(error_msg));
            result->source_name = "";
            result->source_name_length = 0;
            result->content = error_data->c_str();
            result->content_length = error_data->size();
            result->user_data = error_data;
            return result;
        }

        // Read the file
        std::ifstream file(resolved_path, std::ios::binary | std::ios::ate);
        if (!file) {
            std::string error_msg = "Could not open include file: " + path_to_string(resolved_path);
            auto* error_data = new std::string(std::move(error_msg));
            result->source_name = "";
            result->source_name_length = 0;
            result->content = error_data->c_str();
            result->content_length = error_data->size();
            result->user_data = error_data;
            return result;
        }

        auto size = file.tellg();
        file.seekg(0, std::ios::beg);

        auto* content = new std::string();
        content->resize(static_cast<size_t>(size));
        file.read(content->data(), size);

        auto* name = new std::string(path_to_string(resolved_path));

        result->source_name = name->c_str();
        result->source_name_length = name->size();
        result->content = content->c_str();
        result->content_length = content->size();

        // Store both strings in user_data for cleanup
        result->user_data = new std::pair<std::string*, std::string*>(name, content);

        return result;
    }

    void ReleaseInclude(shaderc_include_result* data) override {
        if (data->user_data) {
            // Check if it's an error (single string) or success (pair of strings)
            if (data->source_name_length == 0) {
                delete static_cast<std::string*>(data->user_data);
            } else {
                auto* pair = static_cast<std::pair<std::string*, std::string*>*>(data->user_data);
                delete pair->first;
                delete pair->second;
                delete pair;
            }
        }
        delete data;
    }

private:
    std::vector<std::filesystem::path> m_include_paths;
};

/**
 * @brief Convert RHI shader stage to shaderc shader kind
 */
[[nodiscard]] shaderc_shader_kind to_shaderc_kind(rhi::ShaderStage stage) {
    using enum rhi::ShaderStage;
    switch (stage) {
    case Vertex:
        return shaderc_vertex_shader;
    case Fragment:
        return shaderc_fragment_shader;
    case Geometry:
        return shaderc_geometry_shader;
    case TessellationControl:
        return shaderc_tess_control_shader;
    case TessellationEvaluation:
        return shaderc_tess_evaluation_shader;
    case Compute:
        return shaderc_compute_shader;
    default:
        return shaderc_glsl_infer_from_source;
    }
}

#endif // HZ_VULKAN_BACKEND

} // anonymous namespace

ShaderCompileResult ShaderCompiler::compile(const ShaderCompileOptions& options) {
    ShaderCompileResult result;

#ifdef HZ_VULKAN_BACKEND
    if (options.stage == rhi::ShaderStage::None) {
        result.error_message = "Shader stage must be specified";
        return result;
    }

    shaderc::Compiler compiler;
    shaderc::CompileOptions compile_options;

    // Set optimization level
    switch (options.optimization_level) {
    case 0:
        compile_options.SetOptimizationLevel(shaderc_optimization_level_zero);
        break;
    case 1:
        compile_options.SetOptimizationLevel(shaderc_optimization_level_size);
        break;
    default:
        compile_options.SetOptimizationLevel(shaderc_optimization_level_performance);
        break;
    }

    // Add macro definitions
    for (const auto& [name, value] : options.defines) {
        compile_options.AddMacroDefinition(name, value);
    }

    // Set up include paths
    std::vector<std::filesystem::path> include_paths = options.include_paths;

    // Add the shader's directory to include paths
    if (!options.filename.empty()) {
        std::filesystem::path shader_path(options.filename);
        if (shader_path.has_parent_path()) {
            include_paths.insert(include_paths.begin(), shader_path.parent_path());
        }
    }

    compile_options.SetIncluder(std::make_unique<ShaderIncluder>(std::move(include_paths)));

    // Generate debug info if requested
    if (options.debug_info) {
        compile_options.SetGenerateDebugInfo();
    }

    // Target Vulkan 1.3 / SPIR-V 1.6
    compile_options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_3);
    compile_options.SetTargetSpirv(shaderc_spirv_version_1_6);

    // Compile
    std::string filename_str = options.filename.empty() ? "shader" : std::string(options.filename);

    shaderc::SpvCompilationResult module = compiler.CompileGlslToSpv(
        options.source.data(), options.source.size(), to_shaderc_kind(options.stage),
        filename_str.c_str(), compile_options);

    if (module.GetCompilationStatus() != shaderc_compilation_status_success) {
        result.error_message = module.GetErrorMessage();
        HZ_LOG_ERROR("Shader compilation failed: {}", result.error_message);
        return result;
    }

    // Copy SPIR-V to result
    const auto* spv_begin = reinterpret_cast<const u8*>(module.cbegin());
    const auto* spv_end = reinterpret_cast<const u8*>(module.cend());
    result.spirv.assign(spv_begin, spv_end);
    result.success = true;

    // Include any warnings
    if (module.GetNumWarnings() > 0) {
        result.error_message = module.GetErrorMessage();
    }

#else
    result.error_message = "Shader compilation requires Vulkan backend (HZ_VULKAN_BACKEND)";
    HZ_LOG_ERROR("{}", result.error_message);
#endif

    return result;
}

ShaderCompileResult ShaderCompiler::compile_file(const std::filesystem::path& path,
                                                 rhi::ShaderStage stage) {
    ShaderCompileResult result;

    // Read the file
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        result.error_message = "Could not open shader file: " + path_to_string(path);
        HZ_LOG_ERROR("{}", result.error_message);
        return result;
    }

    auto size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::string source;
    source.resize(static_cast<size_t>(size));
    file.read(source.data(), size);

    // Infer stage if not specified
    if (stage == rhi::ShaderStage::None) {
        stage = infer_stage_from_extension(path);
        if (stage == rhi::ShaderStage::None) {
            result.error_message =
                "Could not infer shader stage from extension: " + path_to_string(path);
            HZ_LOG_ERROR("{}", result.error_message);
            return result;
        }
    }

    // Set up compilation options
    ShaderCompileOptions options;
    options.source = source;
    options.filename = path_to_string(path);
    options.stage = stage;

    // Add common include paths
    if (path.has_parent_path()) {
        options.include_paths.push_back(path.parent_path());

        // Also add common shader library paths
        auto shader_root = path.parent_path().parent_path();
        if (std::filesystem::exists(shader_root / "common")) {
            options.include_paths.push_back(shader_root / "common");
        }
        if (std::filesystem::exists(shader_root / "lib")) {
            options.include_paths.push_back(shader_root / "lib");
        }
    }

    return compile(options);
}

rhi::ShaderStage ShaderCompiler::infer_stage_from_extension(const std::filesystem::path& path) {
    std::string ext = path.extension().string();

    // Convert to lowercase
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (ext == ".vert" || ext == ".vs" || ext == ".vsh") {
        return rhi::ShaderStage::Vertex;
    }
    if (ext == ".frag" || ext == ".fs" || ext == ".fsh" || ext == ".ps") {
        return rhi::ShaderStage::Fragment;
    }
    if (ext == ".geom" || ext == ".gs" || ext == ".gsh") {
        return rhi::ShaderStage::Geometry;
    }
    if (ext == ".tesc" || ext == ".tcs") {
        return rhi::ShaderStage::TessellationControl;
    }
    if (ext == ".tese" || ext == ".tes") {
        return rhi::ShaderStage::TessellationEvaluation;
    }
    if (ext == ".comp" || ext == ".cs" || ext == ".csh") {
        return rhi::ShaderStage::Compute;
    }

    return rhi::ShaderStage::None;
}

} // namespace hz
