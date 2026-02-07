#include "renderer.hpp"

#include "engine/core/log.hpp"
// #include "opengl/framebuffer.hpp"
// #include "opengl/gl_context.hpp"
// #include "opengl/shader.hpp"
// #include "opengl/uniform_buffer.hpp"

#include <random>
#include <string>

#include <glm/gtc/matrix_transform.hpp>

namespace hz {

Renderer::Renderer(rhi::Device& device, rhi::Swapchain& swapchain)
    : m_device(device), m_swapchain(swapchain) {
    // Stub
}

Renderer::~Renderer() noexcept = default;

void Renderer::begin_frame() {
    // Stub
}

void Renderer::end_frame() {
    // Stub
}

rhi::CommandList* Renderer::get_command_list() const {
    return m_current_cmd;
}

void Renderer::submit_lighting(const SceneLighting& lighting) {
    // Stub
}

void Renderer::set_shadow_settings(const ShadowSettings& settings) {
    // Stub
}

void Renderer::begin_shadow_pass(rhi::CommandList& cmd) {
    // Stub
}

void Renderer::end_shadow_pass(rhi::CommandList& cmd) {
    // Stub
}

glm::mat4 Renderer::get_light_space_matrix() const {
    return glm::mat4(1.0f);
}

rhi::TextureView* Renderer::get_shadow_map_view() const {
    return nullptr;
}

std::pair<u32, u32> Renderer::framebuffer_size() const {
    return {1920, 1080};
}

void Renderer::set_clear_color(f32 r, f32 g, f32 b, f32 a) {
    // Stub
}

void Renderer::set_clear_color(const glm::vec4& color) {
    // Stub
}

void Renderer::set_viewport(rhi::CommandList& cmd, i32 x, i32 y, i32 width, i32 height) {
    // Stub
}

void Renderer::resize(u32 width, u32 height) {
    // Stub
}

void Renderer::begin_scene_pass(rhi::CommandList& cmd) {
    // Stub
}

void Renderer::end_scene_pass(rhi::CommandList& cmd) {
    // Stub
}

void Renderer::render_post_process(rhi::CommandList& cmd) {
    // Stub
}

void Renderer::begin_geometry_pass(rhi::CommandList& cmd) {
    // Stub
}

void Renderer::end_geometry_pass(rhi::CommandList& cmd) {
    // Stub
}

rhi::TextureView* Renderer::get_gbuffer_normal_view() const {
    return nullptr;
}

rhi::TextureView* Renderer::get_gbuffer_depth_view() const {
    return nullptr;
}

void Renderer::init_ssao() {
    // Stub
}

void Renderer::render_ssao(rhi::CommandList& cmd, const glm::mat4& projection) {
    // Stub
}

void Renderer::render_ssao_blur(rhi::CommandList& cmd) {
    // Stub
}

rhi::TextureView* Renderer::get_ssao_view() const {
    return nullptr;
}

void Renderer::render_bloom(rhi::CommandList& cmd, float threshold, int blur_passes) {
    // Stub
}

rhi::TextureView* Renderer::get_bloom_view() const {
    return nullptr;
}

void Renderer::update_camera(const glm::mat4& view, const glm::mat4& projection,
                             const glm::vec3& view_pos) {
    // Stub
}

void Renderer::update_scene(float time) {
    // Stub
}

void Renderer::init_quad() {
    // Stub
}

void Renderer::init_ubos() {
    // Stub
}

void Renderer::create_resources() {
    // Stub
}

} // namespace hz
