#include "deferred_renderer.hpp"

#include "engine/assets/asset_registry.hpp"
#include "engine/core/log.hpp"

#include <cmath>
#include <fstream>
#include <sstream>

namespace hz {

// ============================================================================
// GBuffer Implementation
// ============================================================================

void GBuffer::create(rhi::Device& device, u32 w, u32 h) {
    width = w;
    height = h;

    // Create textures
    // Albedo: RGBA16F or RGBA8_SRGB? Header says RGBA16F.
    rhi::TextureDesc desc{};
    desc.width = width;
    desc.height = height;
    desc.usage = rhi::TextureUsage::RenderTarget | rhi::TextureUsage::Sampled;

    // 0: Albedo + Metallic
    desc.format = rhi::Format::RGBA16_FLOAT;
    desc.debug_name = "GBuffer Albedo";
    colors[GBUFFER_ALBEDO_METALLIC] = device.create_texture(desc);
    color_views[GBUFFER_ALBEDO_METALLIC] =
        device.create_texture_view(*colors[GBUFFER_ALBEDO_METALLIC]);

    // 1: Normal + Roughness
    desc.format = rhi::Format::RGBA16_FLOAT;
    desc.debug_name = "GBuffer Normal";
    colors[GBUFFER_NORMAL_ROUGHNESS] = device.create_texture(desc);
    color_views[GBUFFER_NORMAL_ROUGHNESS] =
        device.create_texture_view(*colors[GBUFFER_NORMAL_ROUGHNESS]);

    // 2: Emission + ID
    desc.format = rhi::Format::RGBA16_FLOAT; // Or RGBA8_UNORM
    desc.debug_name = "GBuffer Emission";
    colors[GBUFFER_EMISSION_ID] = device.create_texture(desc);
    color_views[GBUFFER_EMISSION_ID] = device.create_texture_view(*colors[GBUFFER_EMISSION_ID]);

    // 3: Velocity
    desc.format = rhi::Format::RG16_FLOAT;
    desc.debug_name = "GBuffer Velocity";
    colors[GBUFFER_VELOCITY] = device.create_texture(desc);
    color_views[GBUFFER_VELOCITY] = device.create_texture_view(*colors[GBUFFER_VELOCITY]);

    // Depth
    desc.format = rhi::Format::D32_FLOAT;
    desc.usage = rhi::TextureUsage::DepthStencil | rhi::TextureUsage::Sampled;
    desc.debug_name = "GBuffer Depth";
    depth = device.create_texture(desc);
    depth_view = device.create_texture_view(*depth);

    // Framebuffer creation would normally happen here if not using dynamic rendering
    // For now assuming dynamic rendering or deferred FBO management
}

void GBuffer::destroy() {
    // Unique ptrs handle destruction
    fbo.reset();
    for (auto& t : colors)
        t.reset();
    for (auto& v : color_views)
        v.reset();
    depth.reset();
    depth_view.reset();
}

// ============================================================================
// Cascaded Shadow Map Implementation
// ============================================================================

void CascadedShadowMap::create(rhi::Device& device, const CascadedShadowConfig& cfg) {
    config = cfg;

    rhi::TextureDesc desc{};
    desc.width = config.resolution;
    desc.height = config.resolution;
    desc.array_layers = config.cascade_count;
    desc.type = rhi::TextureType::Texture2DArray;
    desc.format = rhi::Format::D32_FLOAT;
    desc.usage = rhi::TextureUsage::DepthStencil | rhi::TextureUsage::Sampled;
    desc.debug_name = "CSM Depth Array";

    depth_array_texture = device.create_texture(desc);
    depth_array_view = device.create_texture_view(*depth_array_texture); // Full array view

    // Create per-layer views and FBOs
    for (u32 i = 0; i < config.cascade_count; ++i) {
        // We need a way to create view for specific layer
        // Assuming create_texture_view supports base_layer/layer_count?
        // If not available in simple API, we might need extended API.
        // For now, placeholder.
    }
}

void CascadedShadowMap::destroy() {
    fbo.reset();
    depth_array_texture.reset();
    depth_array_view.reset();
    for (auto& v : cascade_views)
        v.reset();
    for (auto& f : cascade_fbos)
        f.reset();
}

void CascadedShadowMap::update_cascades(const Camera& camera, const glm::vec3& light_dir) {
    calculate_cascade_splits(camera);
    for (u32 i = 0; i < config.cascade_count; ++i) {
        cascades[i].view_projection = calculate_light_space_matrix(i, camera, light_dir);
    }
}

void CascadedShadowMap::calculate_cascade_splits(const Camera& camera) {
    float near_clip = camera.near_plane;
    float far_clip = camera.far_plane; // Or shadow distance
    // Clamp to shadow distance
    if (far_clip > config.shadow_distance) {
        far_clip = config.shadow_distance;
    }

    float clip_range = far_clip - near_clip;
    float min_z = near_clip;
    float max_z = near_clip + clip_range;

    float range = max_z - min_z;
    float ratio = max_z / min_z;

    // Calculate split depths based on lambda (mix between uniform and logarithmic)
    for (u32 i = 0; i < config.cascade_count; ++i) {
        float p = (i + 1) / static_cast<float>(config.cascade_count);
        float log = min_z * std::pow(ratio, p);
        float uniform = min_z + range * p;
        float d = config.split_lambda * (log - uniform) + uniform;
        cascades[i].split_depth = (d - near_clip) / clip_range;
    }
}

glm::mat4 CascadedShadowMap::calculate_light_space_matrix(u32 cascade, const Camera& camera,
                                                          const glm::vec3& light_dir) {
    // Simplified implementation for brevity
    // In real implementation, this would compute tighter bounds based on cascade frustum
    return glm::mat4(1.0f);
}

// ============================================================================
// SSR Pass
// ============================================================================

void SSRPass::create(rhi::Device& device, u32 w, u32 h, const SSRConfig& cfg) {
    width = w;
    height = h;
    config = cfg;

    rhi::TextureDesc desc{};
    desc.width = width;
    desc.height = height;
    desc.format = rhi::Format::RGBA16_FLOAT;
    desc.usage =
        rhi::TextureUsage::RenderTarget | rhi::TextureUsage::Sampled | rhi::TextureUsage::Storage;
    desc.debug_name = "SSR Result";

    color_texture = device.create_texture(desc);
    color_view = device.create_texture_view(*color_texture);
}

void SSRPass::destroy() {
    fbo.reset();
    color_texture.reset();
    color_view.reset();
}

// ============================================================================
// TAA Pass
// ============================================================================

void TAAPass::create(rhi::Device& device, u32 w, u32 h, const TAAConfig& cfg) {
    width = w;
    height = h;
    config = cfg;
    generate_halton_sequence();

    rhi::TextureDesc desc{};
    desc.width = width;
    desc.height = height;
    desc.format = rhi::Format::RGBA16_FLOAT;
    desc.usage =
        rhi::TextureUsage::RenderTarget | rhi::TextureUsage::Sampled | rhi::TextureUsage::Storage;

    desc.debug_name = "TAA Current";
    current_texture = device.create_texture(desc);
    current_view = device.create_texture_view(*current_texture);

    desc.debug_name = "TAA History";
    history_texture = device.create_texture(desc);
    history_view = device.create_texture_view(*history_texture);

    // Velocity is usually from GBuffer, not created here, referencing GBuffer velocity
}

void TAAPass::destroy() {
    fbo.reset();
    current_texture.reset();
    current_view.reset();
    history_texture.reset();
    history_view.reset();
    velocity_texture.reset();
    velocity_view.reset();
}

void TAAPass::swap_history() {
    // Ping-pong
    std::swap(current_texture, history_texture);
    std::swap(current_view,
              history_view); // Also swap views? Or recreate? Swapping wrapper ptrs is simplest.
}

glm::vec2 TAAPass::get_current_jitter() const {
    if (!config.enabled)
        return glm::vec2(0.0f);
    return jitter_offsets[frame_index % JITTER_SAMPLE_COUNT] * config.jitter_scale;
}

glm::mat4 TAAPass::get_jittered_projection(const glm::mat4& proj) const {
    glm::vec2 jitter = get_current_jitter();
    glm::mat4 result = proj;
    // Apply jitter to projection matrix (usually [2][0] and [2][1])
    result[2][0] += jitter.x / static_cast<float>(width);
    result[2][1] += jitter.y / static_cast<float>(height);
    return result;
}

void TAAPass::generate_halton_sequence() {
    // Placeholder Halton generator
    for (auto& j : jitter_offsets)
        j = glm::vec2(0.0f);
}

// ============================================================================
// Deferred Renderer Implementation
// ============================================================================

DeferredRenderer::DeferredRenderer(rhi::Device& device, rhi::Swapchain& swapchain)
    : m_device(device), m_swapchain(swapchain) {}

DeferredRenderer::~DeferredRenderer() {
    shutdown();
}

bool DeferredRenderer::init() {
    m_width = m_swapchain.width();
    m_height = m_swapchain.height();

    create_fullscreen_quad();
    create_pipelines(); // Need to implement this

    m_gbuffer.create(m_device, m_width, m_height);

    CascadedShadowConfig csm_cfg;
    m_csm.create(m_device, csm_cfg);

    SSRConfig ssr_cfg;
    m_ssr.create(m_device, m_width / 2, m_height / 2, ssr_cfg);

    TAAConfig taa_cfg;
    m_taa.create(m_device, m_width, m_height, taa_cfg);

    m_initialized = true;
    HZ_LOG_INFO("Deferred Renderer Initialized (Vulkan Backed)");
    return true;
}

void DeferredRenderer::shutdown() {
    m_gbuffer.destroy();
    m_csm.destroy();
    m_ssr.destroy();
    m_taa.destroy();
    m_quad_vb.reset();
    m_initialized = false;
}

void DeferredRenderer::resize(u32 width, u32 height) {
    if (width == 0 || height == 0)
        return;
    m_width = width;
    m_height = height;

    m_gbuffer.destroy();
    m_gbuffer.create(m_device, m_width, m_height);

    m_ssr.destroy();
    m_ssr.create(m_device, m_width / 2, m_height / 2, m_ssr.config);

    m_taa.destroy();
    m_taa.create(m_device, m_width, m_height, m_taa.config);

    // Resize other buffers...
}

void DeferredRenderer::begin_geometry_pass(rhi::CommandList& cmd, const Camera& camera) {
    if (!m_initialized)
        return;

    // Use dynamic rendering to bind GBuffer targets
    // cmd.begin_rendering(attachments...);

    rhi::RenderPassBeginInfo info{};
    // Setup color attachments
    // info.color_attachments... = m_gbuffer.color_views...
    // info.depth_attachment = m_gbuffer.depth_view...

    // For now, assume this logic is handled or abstracted
    // cmd.begin_render_pass(m_gbuffer.fbo.get(), ...); // If using FBOs

    // Bind Pipelines
    // cmd.bind_pipeline(m_geometry_pipeline);

    // Helper
    m_csm.update_cascades(camera, glm::vec3(0.0f, -1.0f, 0.0f));
    // m_gbuffer.bind(); // Legacy name, effectively binds targets
}

void DeferredRenderer::end_geometry_pass(rhi::CommandList& cmd) {
    // cmd.end_rendering();
    // m_gbuffer.unbind(); // Legacy
}

void DeferredRenderer::render_shadows(rhi::CommandList& cmd, const glm::vec3& light_direction) {
    // For each cascade...
    // cmd.begin_rendering(cascade_depth_view);
    // Draw shadow casters
    // cmd.end_rendering();
}

void DeferredRenderer::execute_lighting_pass(
    rhi::CommandList& cmd, const Camera& camera, const std::vector<GPUPointLight>& point_lights,
    const std::vector<GPUSpotLight>& spot_lights, const glm::vec3& sun_direction,
    const glm::vec3& sun_color, rhi::TextureView* irradiance_map, rhi::TextureView* prefilter_map,
    rhi::TextureView* brdf_lut, rhi::TextureView* environment_map) {

    // Bind Lighting Pipeline
    // Bind GBuffer textures as descriptors
    // Bind Light UBOs/SSBOs
    render_fullscreen_quad(cmd);
}

void DeferredRenderer::execute_ssr_pass(rhi::CommandList& cmd, const Camera& camera) {
    // Compute shader or fullscreen draw
}

void DeferredRenderer::execute_taa_pass(rhi::CommandList& cmd) {
    // TAA resolve
}

void DeferredRenderer::execute_post_process(rhi::CommandList& cmd, const Camera& camera,
                                            f32 exposure, f32 bloom_threshold,
                                            f32 bloom_intensity) {
    // Bloom chain
    // Tone mapping
}

void DeferredRenderer::render_to_screen(rhi::CommandList& cmd) {
    // Get current swapchain view
    auto* view = m_swapchain.get_current_view();
    auto* texture = m_swapchain.get_current_texture();
    if (view == nullptr || texture == nullptr) {
        return;
    }

    // Transition swapchain image to color attachment
    rhi::TextureBarrier barrier{};
    barrier.texture = texture;
    barrier.old_state = rhi::ResourceState::Undefined;
    barrier.new_state = rhi::ResourceState::RenderTarget;
    cmd.barrier(barrier);

    // Setup dynamic rendering
    rhi::RenderingAttachment color_attachment{};
    color_attachment.view = view;
    color_attachment.load_op = rhi::LoadOp::Clear;
    color_attachment.store_op = rhi::StoreOp::Store;
    color_attachment.clear_value = rhi::ClearColor{0.1F, 0.2F, 0.4F, 1.0F};

    rhi::RenderingInfo render_info{};
    render_info.render_area = {0, 0, m_width, m_height};
    render_info.layer_count = 1;
    render_info.color_attachments = {&color_attachment, 1};

    cmd.begin_rendering(render_info);

    // Set viewport/scissor
    cmd.set_viewport_and_scissor({m_width, m_height});

    // TODO: Blit final result with fullscreen quad
    render_fullscreen_quad(cmd);

    cmd.end_rendering();

    // Transition to present
    barrier.old_state = rhi::ResourceState::RenderTarget;
    barrier.new_state = rhi::ResourceState::Present;
    cmd.barrier(barrier);
}

// =========================================================================
// Configuration
// =========================================================================

void DeferredRenderer::set_csm_config(const CascadedShadowConfig& config) {
    m_csm.config = config;
}
void DeferredRenderer::set_ssr_config(const SSRConfig& config) {
    m_ssr.config = config;
}
void DeferredRenderer::set_taa_config(const TAAConfig& config) {
    m_taa.config = config;
}

void DeferredRenderer::reset_stats() {
    m_stats = RenderStats{};
}

// =========================================================================
// Private
// =========================================================================

void DeferredRenderer::create_pipelines() {
    // TODO: Create RHI pipelines
    // m_geometry_pipeline = device.create_pipeline(...);
    // m_lighting_pipeline = device.create_pipeline(...);
}

void DeferredRenderer::create_fullscreen_quad() {
    float vertices[] = {
        // positions        // texture Coords
        -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
        1.0f,  1.0f, 0.0f, 1.0f, 1.0f, 1.0f,  -1.0f, 0.0f, 1.0f, 0.0f,
    };
    m_quad_vb = m_device.create_vertex_buffer(
        std::span<const u8>(reinterpret_cast<const u8*>(vertices), sizeof(vertices)),
        "Fullscreen Quad VB");
}

void DeferredRenderer::render_fullscreen_quad(rhi::CommandList& cmd) const {
    if (m_quad_vb) {
        // cmd.bind_vertex_buffer(0, *m_quad_vb);
        // cmd.draw(4, 1, 0, 0);
    }
}

void DeferredRenderer::update_frustum(const Camera& camera) {
    // Frustum extraction logic
}

bool DeferredRenderer::is_visible(const glm::vec3& min, const glm::vec3& max) const {
    // AABB test
    return true;
}

[[nodiscard]] u32 DeferredRenderer::get_final_output() const {
    // Legacy mapping or return 0
    return 0; // m_final_texture->get_id(); // ???
}

} // namespace hz
