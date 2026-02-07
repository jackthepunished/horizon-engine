/**
 * @file test_shader_compiler.cpp
 * @brief Unit tests for ShaderCompiler
 */

#include "engine/assets/shader_compiler.hpp"

#include <catch2/catch_test_macros.hpp>


using namespace hz;

// Simple vertex shader for testing
constexpr const char* SIMPLE_VERTEX_SHADER = R"(
#version 450

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec2 in_uv;

layout(location = 0) out vec2 out_uv;

void main() {
    gl_Position = vec4(in_position, 1.0);
    out_uv = in_uv;
}
)";

// Simple fragment shader for testing
constexpr const char* SIMPLE_FRAGMENT_SHADER = R"(
#version 450

layout(location = 0) in vec2 in_uv;
layout(location = 0) out vec4 out_color;

void main() {
    out_color = vec4(in_uv, 0.0, 1.0);
}
)";

// Shader with syntax error
constexpr const char* SHADER_WITH_ERROR = R"(
#version 450

void main() {
    gl_Position = undeclared_variable;  // Error: undeclared identifier
}
)";

// Shader with macro
constexpr const char* SHADER_WITH_MACRO = R"(
#version 450

layout(location = 0) out vec4 out_color;

void main() {
#ifdef USE_RED
    out_color = vec4(1.0, 0.0, 0.0, 1.0);
#else
    out_color = vec4(0.0, 1.0, 0.0, 1.0);
#endif
}
)";

TEST_CASE("ShaderCompiler compiles simple vertex shader", "[shader]") {
    ShaderCompileOptions options;
    options.source = SIMPLE_VERTEX_SHADER;
    options.stage = rhi::ShaderStage::Vertex;
    options.filename = "test_vertex.vert";

    ShaderCompileResult result = ShaderCompiler::compile(options);

    REQUIRE(result.success);
    REQUIRE_FALSE(result.spirv.empty());
    // SPIR-V magic number: 0x07230203
    REQUIRE(result.spirv.size() >= 4);
    u32 magic = *reinterpret_cast<const u32*>(result.spirv.data());
    REQUIRE(magic == 0x07230203);
}

TEST_CASE("ShaderCompiler compiles simple fragment shader", "[shader]") {
    ShaderCompileOptions options;
    options.source = SIMPLE_FRAGMENT_SHADER;
    options.stage = rhi::ShaderStage::Fragment;
    options.filename = "test_fragment.frag";

    ShaderCompileResult result = ShaderCompiler::compile(options);

    REQUIRE(result.success);
    REQUIRE_FALSE(result.spirv.empty());
}

TEST_CASE("ShaderCompiler reports syntax errors", "[shader]") {
    ShaderCompileOptions options;
    options.source = SHADER_WITH_ERROR;
    options.stage = rhi::ShaderStage::Vertex;
    options.filename = "test_error.vert";

    ShaderCompileResult result = ShaderCompiler::compile(options);

    REQUIRE_FALSE(result.success);
    REQUIRE_FALSE(result.error_message.empty());
    // Should mention the undeclared identifier
    REQUIRE(result.error_message.find("undeclared") != std::string::npos);
}

TEST_CASE("ShaderCompiler handles macro definitions", "[shader]") {
    ShaderCompileOptions options;
    options.source = SHADER_WITH_MACRO;
    options.stage = rhi::ShaderStage::Fragment;
    options.filename = "test_macro.frag";
    options.defines.push_back({"USE_RED", "1"});

    ShaderCompileResult result = ShaderCompiler::compile(options);

    REQUIRE(result.success);
    REQUIRE_FALSE(result.spirv.empty());
}

TEST_CASE("ShaderCompiler infers stage from extension", "[shader]") {
    using Stage = rhi::ShaderStage;

    REQUIRE(ShaderCompiler::infer_stage_from_extension("test.vert") == Stage::Vertex);
    REQUIRE(ShaderCompiler::infer_stage_from_extension("test.frag") == Stage::Fragment);
    REQUIRE(ShaderCompiler::infer_stage_from_extension("test.comp") == Stage::Compute);
    REQUIRE(ShaderCompiler::infer_stage_from_extension("test.geom") == Stage::Geometry);
    REQUIRE(ShaderCompiler::infer_stage_from_extension("test.tesc") == Stage::TessControl);
    REQUIRE(ShaderCompiler::infer_stage_from_extension("test.tese") == Stage::TessEval);
    REQUIRE(ShaderCompiler::infer_stage_from_extension("test.unknown") == Stage::None);
}

TEST_CASE("ShaderCompiler requires stage to be specified", "[shader]") {
    ShaderCompileOptions options;
    options.source = SIMPLE_VERTEX_SHADER;
    options.stage = rhi::ShaderStage::None; // Not specified

    ShaderCompileResult result = ShaderCompiler::compile(options);

    REQUIRE_FALSE(result.success);
    REQUIRE_FALSE(result.error_message.empty());
}
