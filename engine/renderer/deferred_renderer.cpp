#include "deferred_renderer.hpp"

#include "engine/assets/asset_registry.hpp"
#include "engine/core/log.hpp"
#include "engine/renderer/deferred_render_data.hpp"

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

    // Create HDR Lighting Texture
    {
        rhi::TextureDesc desc;
        desc.width = m_width;
        desc.height = m_height;
        desc.format = rhi::Format::RGBA16_FLOAT;
        desc.usage = rhi::TextureUsage::RenderTarget | rhi::TextureUsage::Sampled;
        desc.debug_name = "HDR Lighting Result";
        m_lighting_texture = m_device.create_texture(desc);
        m_lighting_view = m_device.create_texture_view(*m_lighting_texture);
    }

    CascadedShadowConfig csm_cfg;
    m_csm.create(m_device, csm_cfg);

    SSRConfig ssr_cfg;
    m_ssr.create(m_device, m_width / 2, m_height / 2, ssr_cfg);

    TAAConfig taa_cfg;
    m_taa.create(m_device, m_width, m_height, taa_cfg);

    // Create point light SSBO
    m_point_light_ssbo =
        m_device.create_buffer({.size = sizeof(GPUPointLight) * kMaxDeferredPointLights,
                                .usage = rhi::BufferUsage::StorageBuffer,
                                .memory = rhi::MemoryUsage::CPU_To_GPU,
                                .debug_name = "PointLightSSBO"});

    update_gbuffer_descriptor_set();

    m_initialized = true;
    HZ_LOG_INFO("Deferred Renderer Initialized (Vulkan Backed)");
    return true;
}

void DeferredRenderer::shutdown() {
    m_gbuffer.destroy();
    m_lighting_texture.reset();
    m_lighting_view.reset();
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
    update_gbuffer_descriptor_set();

    m_lighting_texture.reset();
    m_lighting_view.reset();
    {
        rhi::TextureDesc desc;
        desc.width = m_width;
        desc.height = m_height;
        desc.format = rhi::Format::RGBA16_FLOAT;
        desc.usage = rhi::TextureUsage::RenderTarget | rhi::TextureUsage::Sampled;
        desc.debug_name = "HDR Lighting Result";
        m_lighting_texture = m_device.create_texture(desc);
        m_lighting_view = m_device.create_texture_view(*m_lighting_texture);
    }

    m_ssr.destroy();
    m_ssr.create(m_device, m_width / 2, m_height / 2, m_ssr.config);

    m_taa.destroy();
    m_taa.create(m_device, m_width, m_height, m_taa.config);

    // Resize other buffers...
}

void DeferredRenderer::begin_geometry_pass(rhi::CommandList& cmd, const Camera& camera) {
    if (!m_initialized)
        return;

    const float aspect = static_cast<float>(m_width) / static_cast<float>(m_height);
    const auto camera_ubo = make_deferred_camera_ubo(
        camera.view_matrix(), camera.projection_matrix(aspect), camera.position());
    if (m_camera_ubo) {
        m_camera_ubo->upload(camera_ubo);
    }

    // 2. Transition GBuffer to RenderTarget
    std::vector<rhi::TextureBarrier> barriers;
    barriers.reserve(GBUFFER_COUNT + 1);

    for (u32 i = 0; i < GBUFFER_COUNT; ++i) {
        rhi::TextureBarrier b;
        b.texture = m_gbuffer.colors[i].get();
        b.old_state =
            rhi::ResourceState::ShaderResource; // Assuming it was read last frame or initialized
        b.new_state = rhi::ResourceState::RenderTarget;
        barriers.push_back(b);
    }

    // Depth barrier
    {
        rhi::TextureBarrier b;
        b.texture = m_gbuffer.depth.get();
        b.old_state = rhi::ResourceState::DepthRead; // Or ShaderResource if sampled
        b.new_state = rhi::ResourceState::DepthWrite;
        barriers.push_back(b);
    }

    cmd.barriers({}, barriers);

    // 3. Begin Dynamic Rendering
    rhi::RenderingInfo render_info{};
    render_info.render_area = {0, 0, m_width, m_height};
    render_info.layer_count = 1;

    // Attachments
    std::vector<rhi::RenderingAttachment> color_attachments;
    color_attachments.resize(GBUFFER_COUNT);

    // RT0: Albedo+Metallic (Clear to Black)
    color_attachments[0].view = m_gbuffer.color_views[0].get();
    color_attachments[0].load_op = rhi::LoadOp::Clear;
    color_attachments[0].store_op = rhi::StoreOp::Store;
    color_attachments[0].clear_value = rhi::ClearColor{0.0f, 0.0f, 0.0f, 0.0f};

    // RT1: Normal+Roughness (Clear to 0,0,0,0 -> decoded normal will be invalid but safe)
    // Encoded normal 0,0 decodes to roughly 0,0,-1
    color_attachments[1].view = m_gbuffer.color_views[1].get();
    color_attachments[1].load_op = rhi::LoadOp::Clear;
    color_attachments[1].store_op = rhi::StoreOp::Store;
    color_attachments[1].clear_value =
        rhi::ClearColor{0.5f, 0.5f, 0.0f, 0.0f}; // 0.5,0.5 encodes normal (0,0,1)

    // RT2: Emission+ID (Clear to Black)
    color_attachments[2].view = m_gbuffer.color_views[2].get();
    color_attachments[2].load_op = rhi::LoadOp::Clear;
    color_attachments[2].store_op = rhi::StoreOp::Store;
    color_attachments[2].clear_value = rhi::ClearColor{0.0f, 0.0f, 0.0f, 0.0f};

    // RT3: Velocity (Clear to Black)
    color_attachments[3].view = m_gbuffer.color_views[3].get();
    color_attachments[3].load_op = rhi::LoadOp::Clear;
    color_attachments[3].store_op = rhi::StoreOp::Store;
    color_attachments[3].clear_value = rhi::ClearColor{0.0f, 0.0f, 0.0f, 0.0f};

    render_info.color_attachments = color_attachments;

    // Depth attachment
    rhi::RenderingAttachment depth_attachment{};
    depth_attachment.view = m_gbuffer.depth_view.get();
    depth_attachment.load_op = rhi::LoadOp::Clear;
    depth_attachment.store_op = rhi::StoreOp::Store;
    depth_attachment.clear_value = rhi::ClearDepthStencil{1.0f, 0};

    render_info.depth_attachment = &depth_attachment;

    cmd.begin_rendering(render_info);

    // 4. Bind Pipeline & State
    cmd.bind_pipeline(*m_geometry_pipeline);
    cmd.set_viewport_and_scissor({m_width, m_height});

    // 5. Bind Camera Descriptor Set
    cmd.bind_descriptor_set(*m_geometry_layout, 0, *m_camera_set);

    m_csm.update_cascades(
        camera, glm::vec3(0.0f, -1.0f, 0.0f)); // Update shadow cascades (placeholder light dir)
}

void DeferredRenderer::end_geometry_pass(rhi::CommandList& cmd) {
    cmd.end_rendering();

    // Transition GBuffer to ShaderResource for lighting pass
    std::vector<rhi::TextureBarrier> barriers;
    barriers.reserve(GBUFFER_COUNT + 1);

    for (u32 i = 0; i < GBUFFER_COUNT; ++i) {
        rhi::TextureBarrier b;
        b.texture = m_gbuffer.colors[i].get();
        b.old_state = rhi::ResourceState::RenderTarget;
        b.new_state = rhi::ResourceState::ShaderResource;
        barriers.push_back(b);
    }

    // Depth barrier
    {
        rhi::TextureBarrier b;
        b.texture = m_gbuffer.depth.get();
        b.old_state = rhi::ResourceState::DepthWrite;
        b.new_state = rhi::ResourceState::ShaderResource; // For depth sampling in lighting
        barriers.push_back(b);
    }

    cmd.barriers({}, barriers);
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

    if (!m_initialized)
        return;

    const u32 point_light_count = static_cast<u32>(
        std::min(point_lights.size(), static_cast<size_t>(kMaxDeferredPointLights)));
    const u32 spot_light_count =
        static_cast<u32>(std::min(spot_lights.size(), static_cast<size_t>(kMaxDeferredSpotLights)));
    const auto light_ubo = make_deferred_light_ubo(sun_direction, sun_color, 1.0f,
                                                   point_light_count, spot_light_count);
    if (m_light_ubo) {
        m_light_ubo->upload(light_ubo);
    }

    if (point_light_count > 0 && m_point_light_ssbo) {
        m_point_light_ssbo->upload(
            std::span<const GPUPointLight>(point_lights.data(), point_light_count));
    }

    // 2. Transition Lighting Texture to RenderTarget
    {
        rhi::TextureBarrier b;
        b.texture = m_lighting_texture.get();
        b.old_state = rhi::ResourceState::Undefined;
        b.new_state = rhi::ResourceState::RenderTarget;
        cmd.barrier(b);
    }

    // 3. Begin Dynamic Rendering
    rhi::RenderingInfo render_info{};
    render_info.render_area = {0, 0, m_width, m_height};
    render_info.layer_count = 1;

    rhi::RenderingAttachment color_att{};
    color_att.view = m_lighting_view.get();
    color_att.load_op = rhi::LoadOp::Clear;
    color_att.store_op = rhi::StoreOp::Store;
    color_att.clear_value = rhi::ClearColor{0.0f, 0.0f, 0.0f, 1.0f};

    render_info.color_attachments = {&color_att, 1};

    cmd.begin_rendering(render_info);

    // 4. Bind Pipeline
    cmd.bind_pipeline(*m_lighting_pipeline);
    cmd.set_viewport_and_scissor({m_width, m_height});

    // 5. Bind Descriptor Sets
    cmd.bind_descriptor_set(*m_lighting_layout, 0, *m_camera_set);
    cmd.bind_descriptor_set(*m_lighting_layout, 1, *m_gbuffer_input_set);
    cmd.bind_descriptor_set(*m_lighting_layout, 2, *m_lighting_data_set);

    // 6. Draw
    render_fullscreen_quad(cmd);

    // 7. End
    cmd.end_rendering();

    // 8. Transition to ShaderResource
    {
        rhi::TextureBarrier b;
        b.texture = m_lighting_texture.get();
        b.old_state = rhi::ResourceState::RenderTarget;
        b.new_state = rhi::ResourceState::ShaderResource;
        cmd.barrier(b);
    }
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

    // Bind Composite Pipeline
    cmd.bind_pipeline(*m_composite_pipeline);

    // Update & Bind Descriptor Set
    m_composite_input_set->write_texture(0, *m_lighting_view, *m_nearest_sampler);
    cmd.bind_descriptor_set(*m_composite_layout, 0, *m_composite_input_set);

    // Push Constants (Exposure)
    f32 exposure = 1.0f;
    cmd.push_constants(*m_composite_layout, rhi::ShaderStage::Fragment, exposure);

    render_fullscreen_quad(cmd);

    cmd.end_rendering();

    // Transition to present
    barrier.old_state = rhi::ResourceState::RenderTarget;
    barrier.new_state = rhi::ResourceState::Present;
    cmd.barrier(barrier);
}

// ============================================================================
// Private helpers
// ============================================================================

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
    // =========================================================================
    // Descriptor Set Layouts
    // =========================================================================

    // Camera layout (set 0, binding 0: CameraUBO)
    {
        rhi::DescriptorSetLayoutDesc desc;
        desc.bindings.push_back(rhi::DescriptorBinding::uniform_buffer(
            0, rhi::ShaderStage::Vertex | rhi::ShaderStage::Fragment));
        m_camera_layout = m_device.create_descriptor_set_layout(desc);
    }

    // Material layout (set 1: albedo, normal, ARM textures)
    {
        rhi::DescriptorSetLayoutDesc desc;
        desc.bindings.push_back(
            rhi::DescriptorBinding::combined_image_sampler(0, rhi::ShaderStage::Fragment));
        desc.bindings.push_back(
            rhi::DescriptorBinding::combined_image_sampler(1, rhi::ShaderStage::Fragment));
        desc.bindings.push_back(
            rhi::DescriptorBinding::combined_image_sampler(2, rhi::ShaderStage::Fragment));
        m_material_layout = m_device.create_descriptor_set_layout(desc);
    }

    // GBuffer input layout (set 1: GBuffer textures for lighting pass)
    {
        rhi::DescriptorSetLayoutDesc desc;
        desc.bindings.push_back(
            rhi::DescriptorBinding::combined_image_sampler(0, rhi::ShaderStage::Fragment));
        desc.bindings.push_back(
            rhi::DescriptorBinding::combined_image_sampler(1, rhi::ShaderStage::Fragment));
        desc.bindings.push_back(
            rhi::DescriptorBinding::combined_image_sampler(2, rhi::ShaderStage::Fragment));
        desc.bindings.push_back(
            rhi::DescriptorBinding::combined_image_sampler(3, rhi::ShaderStage::Fragment));
        m_gbuffer_input_layout = m_device.create_descriptor_set_layout(desc);
    }

    // Lighting data layout (set 2: LightUBO + PointLightSSBO)
    {
        rhi::DescriptorSetLayoutDesc desc;
        desc.bindings.push_back(
            rhi::DescriptorBinding::uniform_buffer(0, rhi::ShaderStage::Fragment));
        desc.bindings.push_back(
            rhi::DescriptorBinding::storage_buffer(1, rhi::ShaderStage::Fragment));
        m_lighting_data_layout = m_device.create_descriptor_set_layout(desc);
    }

    // Composite input layout (set 0: HDR result texture)
    {
        rhi::DescriptorSetLayoutDesc desc;
        desc.bindings.push_back(
            rhi::DescriptorBinding::combined_image_sampler(0, rhi::ShaderStage::Fragment));
        m_composite_input_layout = m_device.create_descriptor_set_layout(desc);
    }

    // =========================================================================
    // Pipeline Layouts
    // =========================================================================

    // Geometry pipeline layout: set 0 (camera), set 1 (material), push constants (model)
    {
        rhi::PipelineLayoutDesc desc;
        desc.set_layouts.push_back(m_camera_layout.get());
        desc.set_layouts.push_back(m_material_layout.get());
        rhi::PushConstantRange pc_range;
        pc_range.stages = rhi::ShaderStage::Vertex;
        pc_range.offset = 0;
        pc_range.size = sizeof(glm::mat4);
        desc.push_constant_ranges.push_back(pc_range);
        m_geometry_layout = m_device.create_pipeline_layout(desc);
    }

    // Lighting pipeline layout: set 0 (camera), set 1 (GBuffer), set 2 (lights)
    {
        rhi::PipelineLayoutDesc desc;
        desc.set_layouts.push_back(m_camera_layout.get());
        desc.set_layouts.push_back(m_gbuffer_input_layout.get());
        desc.set_layouts.push_back(m_lighting_data_layout.get());
        m_lighting_layout = m_device.create_pipeline_layout(desc);
    }

    // Composite pipeline layout: set 0 (HDR), push constants (exposure)
    {
        rhi::PipelineLayoutDesc desc;
        desc.set_layouts.push_back(m_composite_input_layout.get());
        rhi::PushConstantRange pc_range;
        pc_range.stages = rhi::ShaderStage::Fragment;
        pc_range.offset = 0;
        pc_range.size = sizeof(f32);
        desc.push_constant_ranges.push_back(pc_range);
        m_composite_layout = m_device.create_pipeline_layout(desc);
    }

    // =========================================================================
    // Shader Modules
    // =========================================================================

    auto vk_geometry_vert = m_device.create_shader_from_file(
        "assets/shaders/deferred/vk_geometry.vert", rhi::ShaderStage::Vertex, "GeometryVert");
    auto vk_geometry_frag = m_device.create_shader_from_file(
        "assets/shaders/deferred/vk_geometry.frag", rhi::ShaderStage::Fragment, "GeometryFrag");
    auto vk_fullscreen_vert = m_device.create_shader_from_file(
        "assets/shaders/deferred/vk_fullscreen.vert", rhi::ShaderStage::Vertex, "FullscreenVert");
    auto vk_lighting_frag = m_device.create_shader_from_file(
        "assets/shaders/deferred/vk_lighting.frag", rhi::ShaderStage::Fragment, "LightingFrag");
    auto vk_composite_frag = m_device.create_shader_from_file(
        "assets/shaders/deferred/vk_composite.frag", rhi::ShaderStage::Fragment, "CompositeFrag");

    if (!vk_geometry_vert || !vk_geometry_frag || !vk_fullscreen_vert || !vk_lighting_frag ||
        !vk_composite_frag) {
        HZ_LOG_ERROR("Failed to load one or more shaders");
        return;
    }

    // =========================================================================
    // Geometry Pipeline
    // =========================================================================
    {
        rhi::GraphicsPipelineDesc desc;
        desc.vertex_shader = vk_geometry_vert.get();
        desc.fragment_shader = vk_geometry_frag.get();
        desc.vertex_layout = rhi::VertexInputLayout::standard_vertex();
        desc.topology = rhi::PrimitiveTopology::TriangleList;
        desc.rasterization = rhi::RasterizationState::default_state();
        desc.depth_stencil = rhi::DepthStencilState::default_state();
        desc.blend = rhi::BlendState::disabled(4);
        desc.multisample = {};
        desc.layout = m_geometry_layout.get();
        m_geometry_pipeline = m_device.create_graphics_pipeline(desc);
    }

    // =========================================================================
    // Lighting Pipeline
    // =========================================================================
    {
        rhi::GraphicsPipelineDesc desc;
        desc.vertex_shader = vk_fullscreen_vert.get();
        desc.fragment_shader = vk_lighting_frag.get();
        desc.vertex_layout = rhi::VertexInputLayout::position_uv();
        desc.topology = rhi::PrimitiveTopology::TriangleStrip;
        desc.rasterization = rhi::RasterizationState::no_cull();
        desc.depth_stencil = rhi::DepthStencilState::disabled();
        desc.blend = rhi::BlendState::disabled(1);
        desc.multisample = {};
        desc.layout = m_lighting_layout.get();
        m_lighting_pipeline = m_device.create_graphics_pipeline(desc);
    }

    // =========================================================================
    // Composite Pipeline
    // =========================================================================
    {
        rhi::GraphicsPipelineDesc desc;
        desc.vertex_shader = vk_fullscreen_vert.get();
        desc.fragment_shader = vk_composite_frag.get();
        desc.vertex_layout = rhi::VertexInputLayout::position_uv();
        desc.topology = rhi::PrimitiveTopology::TriangleStrip;
        desc.rasterization = rhi::RasterizationState::no_cull();
        desc.depth_stencil = rhi::DepthStencilState::disabled();
        desc.blend = rhi::BlendState::disabled(1);
        desc.multisample = {};
        desc.layout = m_composite_layout.get();
        m_composite_pipeline = m_device.create_graphics_pipeline(desc);
    }

    // =========================================================================
    // Descriptor Pool & Samplers
    // =========================================================================

    {
        rhi::DescriptorPoolDesc desc;
        desc.max_sets = 8;
        desc.pool_sizes.push_back({rhi::DescriptorType::UniformBuffer, 4});
        desc.pool_sizes.push_back({rhi::DescriptorType::StorageBuffer, 2});
        desc.pool_sizes.push_back({rhi::DescriptorType::CombinedImageSampler, 12});
        desc.pool_sizes.push_back({rhi::DescriptorType::SampledImage, 8});
        m_descriptor_pool = m_device.create_descriptor_pool(desc);
    }

    // Allocate descriptor sets
    m_camera_set = m_descriptor_pool->allocate(*m_camera_layout);
    m_gbuffer_input_set = m_descriptor_pool->allocate(*m_gbuffer_input_layout);
    m_lighting_data_set = m_descriptor_pool->allocate(*m_lighting_data_layout);
    m_composite_input_set = m_descriptor_pool->allocate(*m_composite_input_layout);

    // Create UBO buffers
    m_camera_ubo = m_device.create_uniform_buffer(sizeof(DeferredCameraUBO), "CameraUBO");
    m_light_ubo = m_device.create_uniform_buffer(sizeof(DeferredLightUBO), "LightUBO");

    m_camera_set->write_buffer(0, *m_camera_ubo);
    m_lighting_data_set->write_buffer(0, *m_light_ubo);
    if (m_point_light_ssbo) {
        m_lighting_data_set->write_storage_buffer(1, *m_point_light_ssbo);
    }

    // Create samplers
    {
        rhi::SamplerDesc desc;
        desc.min_filter = rhi::Filter::Linear;
        desc.mag_filter = rhi::Filter::Linear;
        desc.mipmap_mode = rhi::MipmapMode::Linear;
        desc.address_u = rhi::AddressMode::Repeat;
        desc.address_v = rhi::AddressMode::Repeat;
        desc.address_w = rhi::AddressMode::Repeat;
        desc.max_anisotropy = 16.0f;
        desc.debug_name = "LinearSampler";
        m_linear_sampler = m_device.create_sampler(desc);
    }

    {
        rhi::SamplerDesc desc;
        desc.min_filter = rhi::Filter::Nearest;
        desc.mag_filter = rhi::Filter::Nearest;
        desc.mipmap_mode = rhi::MipmapMode::Nearest;
        desc.address_u = rhi::AddressMode::ClampToEdge;
        desc.address_v = rhi::AddressMode::ClampToEdge;
        desc.address_w = rhi::AddressMode::ClampToEdge;
        desc.debug_name = "NearestSampler";
        m_nearest_sampler = m_device.create_sampler(desc);
    }

    HZ_LOG_INFO("Deferred renderer pipelines created");
}

void DeferredRenderer::update_gbuffer_descriptor_set() {
    if (!m_gbuffer_input_set || !m_nearest_sampler) {
        return;
    }

    m_gbuffer_input_set->write_texture(0, *m_gbuffer.color_views[GBUFFER_ALBEDO_METALLIC],
                                       *m_nearest_sampler);
    m_gbuffer_input_set->write_texture(1, *m_gbuffer.color_views[GBUFFER_NORMAL_ROUGHNESS],
                                       *m_nearest_sampler);
    m_gbuffer_input_set->write_texture(2, *m_gbuffer.color_views[GBUFFER_EMISSION_ID],
                                       *m_nearest_sampler);
    m_gbuffer_input_set->write_texture(3, *m_gbuffer.depth_view, *m_nearest_sampler);
}

void DeferredRenderer::create_fullscreen_quad() {
    float vertices[] = {
        // positions        // texture Coords
        -1.0f, 1.0f,  0.0f, 0.0f, 1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
        1.0f,  -1.0f, 0.0f, 1.0f, 0.0f, -1.0f, 1.0f,  0.0f, 0.0f, 1.0f,
        1.0f,  -1.0f, 0.0f, 1.0f, 0.0f, 1.0f,  1.0f,  0.0f, 1.0f, 1.0f,
    };
    m_quad_vb = m_device.create_vertex_buffer(
        std::span<const u8>(reinterpret_cast<const u8*>(vertices), sizeof(vertices)),
        "Fullscreen Quad VB");
}

void DeferredRenderer::render_fullscreen_quad(rhi::CommandList& cmd) const {
    if (m_quad_vb) {
        cmd.bind_vertex_buffer(0, *m_quad_vb);
        cmd.draw(6, 1, 0, 0);
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
