#include "deferred_renderer.hpp"

#include "engine/assets/asset_registry.hpp"
#include "engine/core/log.hpp"
#include "engine/renderer/deferred_render_data.hpp"

#include <cfloat>
#include <cmath>
#include <fstream>
#include <random>
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

    // Create per-layer views
    for (u32 i = 0; i < config.cascade_count; ++i) {
        rhi::TextureViewDesc view_desc;
        view_desc.texture = depth_array_texture.get();
        view_desc.view_type = rhi::TextureType::Texture2D;
        view_desc.format = desc.format;
        view_desc.base_mip_level = 0;
        view_desc.mip_level_count = 1;
        view_desc.base_array_layer = i;
        view_desc.array_layer_count = 1;
        view_desc.debug_name = "CSM Layer View";
        cascade_views[i] = device.create_texture_view(view_desc);
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

void CascadedShadowMap::update_cascades(const Camera& camera, const glm::vec3& light_dir,
                                        f32 aspect_ratio) {
    calculate_cascade_splits(camera);
    for (u32 i = 0; i < config.cascade_count; ++i) {
        cascades[i].view_projection =
            calculate_light_space_matrix(i, camera, light_dir, aspect_ratio);
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
        float p = static_cast<float>(i + 1) / static_cast<float>(config.cascade_count);
        float log = min_z * std::pow(ratio, p);
        float uniform = min_z + range * p;
        float d = config.split_lambda * (log - uniform) + uniform;
        cascades[i].split_depth = d;
    }
}

glm::mat4 CascadedShadowMap::calculate_light_space_matrix(u32 cascade, const Camera& camera,
                                                          const glm::vec3& light_dir,
                                                          f32 aspect_ratio) {
    float near_clip = camera.near_plane;
    float far_clip = camera.far_plane;
    if (far_clip > config.shadow_distance) {
        far_clip = config.shadow_distance;
    }

    float prev_split_dist = cascade == 0 ? near_clip : cascades[cascade - 1].split_depth;
    float split_dist = cascades[cascade].split_depth;

    float tan_half_fov = std::tan(glm::radians(camera.fov * 0.5f));
    float near_height = tan_half_fov * prev_split_dist;
    float near_width = near_height * aspect_ratio;
    float far_height = tan_half_fov * split_dist;
    float far_width = far_height * aspect_ratio;

    const glm::vec3 cam_pos = camera.position();
    const glm::vec3 cam_forward = glm::normalize(camera.front());
    const glm::vec3 cam_right = glm::normalize(camera.right());
    const glm::vec3 cam_up = glm::normalize(glm::cross(cam_right, cam_forward));

    glm::vec3 near_center = cam_pos + cam_forward * prev_split_dist;
    glm::vec3 far_center = cam_pos + cam_forward * split_dist;

    std::array<glm::vec3, 8> frustum_corners = {
        near_center + cam_up * near_height - cam_right * near_width,
        near_center + cam_up * near_height + cam_right * near_width,
        near_center - cam_up * near_height - cam_right * near_width,
        near_center - cam_up * near_height + cam_right * near_width,
        far_center + cam_up * far_height - cam_right * far_width,
        far_center + cam_up * far_height + cam_right * far_width,
        far_center - cam_up * far_height - cam_right * far_width,
        far_center - cam_up * far_height + cam_right * far_width,
    };

    glm::vec3 frustum_center(0.0f);
    for (const auto& corner : frustum_corners) {
        frustum_center += corner;
    }
    frustum_center /= static_cast<float>(frustum_corners.size());

    glm::vec3 light_dir_norm = glm::normalize(light_dir);
    glm::vec3 light_pos = frustum_center - light_dir_norm * (config.shadow_distance * 0.5f);
    glm::vec3 up_dir = glm::vec3(0.0f, 1.0f, 0.0f);
    if (std::abs(glm::dot(up_dir, light_dir_norm)) > 0.99f) {
        up_dir = glm::vec3(0.0f, 0.0f, 1.0f);
    }

    glm::mat4 light_view = glm::lookAt(light_pos, frustum_center, up_dir);

    glm::vec3 min_bounds(FLT_MAX);
    glm::vec3 max_bounds(-FLT_MAX);

    for (const auto& corner : frustum_corners) {
        glm::vec4 corner_ls = light_view * glm::vec4(corner, 1.0f);
        min_bounds = glm::min(min_bounds, glm::vec3(corner_ls));
        max_bounds = glm::max(max_bounds, glm::vec3(corner_ls));
    }

    float extent_x = max_bounds.x - min_bounds.x;
    float extent_y = max_bounds.y - min_bounds.y;
    float texel_size_x = extent_x / static_cast<float>(config.resolution);
    float texel_size_y = extent_y / static_cast<float>(config.resolution);

    min_bounds.x = std::floor(min_bounds.x / texel_size_x) * texel_size_x;
    min_bounds.y = std::floor(min_bounds.y / texel_size_y) * texel_size_y;
    max_bounds.x = std::ceil(max_bounds.x / texel_size_x) * texel_size_x;
    max_bounds.y = std::ceil(max_bounds.y / texel_size_y) * texel_size_y;

    float z_mult = 10.0f;
    if (min_bounds.z < 0.0f) {
        min_bounds.z *= z_mult;
    } else {
        min_bounds.z /= z_mult;
    }

    if (max_bounds.z < 0.0f) {
        max_bounds.z /= z_mult;
    } else {
        max_bounds.z *= z_mult;
    }

    glm::mat4 light_proj = glm::ortho(min_bounds.x, max_bounds.x, min_bounds.y, max_bounds.y,
                                      min_bounds.z, max_bounds.z);

    return light_proj * light_view;
}

// ============================================================================
// SSAO Pass
// ============================================================================

void SSAOPass::create(rhi::Device& device, u32 w, u32 h, const SSAOConfig& cfg,
                      const rhi::DescriptorSetLayout& camera_layout,
                      const rhi::DescriptorSetLayout& gbuffer_layout) {
    width = static_cast<u32>(w * cfg.resolution_scale);
    height = static_cast<u32>(h * cfg.resolution_scale);
    config = cfg;

    // 1. Create Textures
    {
        rhi::TextureDesc desc{};
        desc.width = width;
        desc.height = height;
        desc.format = rhi::Format::R8_UNORM;
        desc.usage = rhi::TextureUsage::RenderTarget | rhi::TextureUsage::Sampled;
        desc.debug_name = "SSAO Raw";
        color_texture = device.create_texture(desc);
        color_view = device.create_texture_view(*color_texture);

        desc.debug_name = "SSAO Blur";
        blur_texture = device.create_texture(desc);
        blur_view = device.create_texture_view(*blur_texture);
    }

    // 2. Generate Kernel
    std::vector<glm::vec4> kernel;
    {
        std::uniform_real_distribution<float> random_floats(0.0, 1.0);
        std::default_random_engine generator;
        for (int i = 0; i < 64; ++i) {
            glm::vec3 sample(random_floats(generator) * 2.0 - 1.0,
                             random_floats(generator) * 2.0 - 1.0, random_floats(generator));
            sample = glm::normalize(sample);
            sample *= random_floats(generator);
            float scale = float(i) / 64.0;
            scale = 0.1f + (scale * scale) * (1.0f - 0.1f);
            sample *= scale;
            kernel.push_back(glm::vec4(sample, 0.0f));
        }
    }
    kernel_ubo = device.create_buffer(
        {sizeof(glm::vec4) * 64, rhi::BufferUsage::UniformBuffer, rhi::MemoryUsage::CPU_To_GPU});
    void* data = kernel_ubo->map();
    memcpy(data, kernel.data(), sizeof(glm::vec4) * 64);
    kernel_ubo->unmap();

    // 3. Generate Noise Texture
    {
        std::vector<glm::vec4> noise;
        std::uniform_real_distribution<float> random_floats(0.0, 1.0);
        std::default_random_engine generator;
        for (unsigned int i = 0; i < 16; i++) {
            glm::vec4 sample(random_floats(generator) * 2.0 - 1.0,
                             random_floats(generator) * 2.0 - 1.0, 0.0f, 0.0f);
            noise.push_back(sample);
        }

        rhi::TextureDesc desc{};
        desc.width = 4;
        desc.height = 4;
        desc.format = rhi::Format::RGBA16_FLOAT;
        desc.usage = rhi::TextureUsage::Sampled | rhi::TextureUsage::TransferDst;
        desc.debug_name = "SSAO Noise";
        noise_texture = device.create_texture(desc);
        noise_view = device.create_texture_view(*noise_texture);
        device.update_texture(*noise_texture, noise.data(),
                              static_cast<u64>(noise.size() * sizeof(glm::vec4)));
    }

    params_ubo = device.create_buffer(
        {sizeof(SSAOConfig), rhi::BufferUsage::UniformBuffer, rhi::MemoryUsage::CPU_To_GPU});

    // 4. Create Descriptors & Pipelines
    // SSAO Generation
    {
        rhi::DescriptorSetLayoutDesc desc;
        desc.bindings.push_back(
            rhi::DescriptorBinding::uniform_buffer(0, rhi::ShaderStage::Fragment)); // Kernel
        desc.bindings.push_back(
            rhi::DescriptorBinding::uniform_buffer(1, rhi::ShaderStage::Fragment)); // Params
        desc.bindings.push_back(
            rhi::DescriptorBinding::combined_image_sampler(2, rhi::ShaderStage::Fragment)); // Noise
        descriptor_layout = device.create_descriptor_set_layout(desc);
    }

    // Create Repeat Sampler
    auto sampler = device.create_sampler({rhi::Filter::Nearest, rhi::Filter::Nearest,
                                          rhi::MipmapMode::Nearest, rhi::AddressMode::Repeat,
                                          rhi::AddressMode::Repeat, rhi::AddressMode::Repeat});

    // 5. Descriptor Pool
    {
        rhi::DescriptorPoolDesc desc{};
        desc.pool_sizes = {{rhi::DescriptorType::UniformBuffer, 2},
                           {rhi::DescriptorType::CombinedImageSampler, 2}};
        desc.max_sets = 2; // SSAO Set + Blur Set
        descriptor_pool = device.create_descriptor_pool(desc);
    }

    // 6. SSAO Descriptors & Pipeline
    {
        // Create Pipeline Layout
        rhi::PipelineLayoutDesc pl_desc{};
        pl_desc.set_layouts = {&camera_layout, &gbuffer_layout, descriptor_layout.get()};
        pipeline_layout = device.create_pipeline_layout(pl_desc);

        // Create Pipeline (using temp RenderPass for compatibility)
        auto rp_desc = rhi::RenderPassDesc::simple(rhi::Format::R8_UNORM);
        auto temp_rp = device.create_render_pass(rp_desc);

        rhi::GraphicsPipelineDesc pipe_desc{};
        pipe_desc.layout = pipeline_layout.get();
        pipe_desc.render_pass = temp_rp.get();

        rhi::VertexInputLayout item_layout;
        item_layout.bindings.push_back({0, 5 * sizeof(float), rhi::VertexInputRate::Vertex});
        item_layout.attributes.push_back({0, 0, rhi::Format::RGB32_FLOAT, 0});
        item_layout.attributes.push_back({1, 0, rhi::Format::RG32_FLOAT, 3 * sizeof(float)});
        pipe_desc.vertex_layout = item_layout;

        auto vs = device.create_shader_from_file("assets/shaders/deferred/vk_fullscreen.vert",
                                                 rhi::ShaderStage::Vertex, "FullscreenVert");
        auto fs = device.create_shader_from_file("assets/shaders/ssao.frag",
                                                 rhi::ShaderStage::Fragment, "SSAOFrag");

        if (vs && fs) {
            pipe_desc.vertex_shader = vs.get();
            pipe_desc.fragment_shader = fs.get();
            pipe_desc.topology = rhi::PrimitiveTopology::TriangleList;
            pipeline = device.create_graphics_pipeline(pipe_desc);
        }

        descriptor_set = descriptor_pool->allocate(*descriptor_layout);
        descriptor_set->write_buffer(0, *kernel_ubo);
        descriptor_set->write_buffer(1, *params_ubo);
        descriptor_set->write_texture(2, *noise_view, *sampler);
    }

    // 7. Blur Descriptors & Pipeline
    {
        // Layout: Set 0 (SSAO Input)
        rhi::DescriptorSetLayoutDesc desc;
        desc.bindings.push_back(
            rhi::DescriptorBinding::combined_image_sampler(0, rhi::ShaderStage::Fragment));
        blur_descriptor_layout = device.create_descriptor_set_layout(desc);

        blur_descriptor_set = descriptor_pool->allocate(*blur_descriptor_layout);

        auto linear_sampler =
            device.create_sampler({rhi::Filter::Linear, rhi::Filter::Linear,
                                   rhi::MipmapMode::Linear, rhi::AddressMode::ClampToEdge,
                                   rhi::AddressMode::ClampToEdge, rhi::AddressMode::ClampToEdge});
        blur_descriptor_set->write_texture(0, *color_view, *linear_sampler);

        // Pipeline Layout
        rhi::PipelineLayoutDesc pl_desc{};
        pl_desc.set_layouts = {blur_descriptor_layout.get()};
        blur_layout = device.create_pipeline_layout(pl_desc);

        // Pipeline
        auto rp_desc = rhi::RenderPassDesc::simple(rhi::Format::R8_UNORM);
        auto temp_rp = device.create_render_pass(rp_desc);

        rhi::GraphicsPipelineDesc pipe_desc{};
        pipe_desc.layout = blur_layout.get();
        pipe_desc.render_pass = temp_rp.get();

        auto vs = device.create_shader_from_file("assets/shaders/deferred/vk_fullscreen.vert",
                                                 rhi::ShaderStage::Vertex, "FullscreenVert");
        auto fs = device.create_shader_from_file("assets/shaders/ssao_blur.frag",
                                                 rhi::ShaderStage::Fragment, "SSAOBlurFrag");

        if (vs && fs) {
            pipe_desc.vertex_shader = vs.get();
            pipe_desc.fragment_shader = fs.get();
            pipe_desc.topology = rhi::PrimitiveTopology::TriangleList;
            rhi::VertexInputLayout item_layout;
            item_layout.bindings.push_back({0, 5 * sizeof(float), rhi::VertexInputRate::Vertex});
            item_layout.attributes.push_back({0, 0, rhi::Format::RGB32_FLOAT, 0});
            item_layout.attributes.push_back({1, 0, rhi::Format::RG32_FLOAT, 3 * sizeof(float)});
            pipe_desc.vertex_layout = item_layout;

            blur_pipeline = device.create_graphics_pipeline(pipe_desc);
        }
    }
}

void SSAOPass::destroy() {
    color_texture.reset();
    color_view.reset();
    blur_texture.reset();
    blur_view.reset();
    noise_texture.reset();
    noise_view.reset();
    kernel_ubo.reset();
    params_ubo.reset();
    pipeline.reset();
    pipeline_layout.reset();
    descriptor_layout.reset();
    descriptor_set.reset();
    blur_pipeline.reset();
    blur_layout.reset();
    blur_descriptor_layout.reset();
    blur_descriptor_set.reset();
    descriptor_pool.reset();
}

void SSAOPass::execute(rhi::CommandList& cmd, rhi::Device& device, const Camera& camera,
                       const rhi::Buffer& quad_vb, const rhi::DescriptorSet& camera_set,
                       const rhi::DescriptorSet& gbuffer_set) {
    if (!config.enabled || !pipeline || !blur_pipeline)
        return;

    // Update Params
    {
        SSAOConfig* mapped = (SSAOConfig*)params_ubo->map();
        if (mapped) {
            struct SSAOParams {
                glm::vec2 noise_scale;
                float radius;
                float bias;
                int kernel_size;
                float power;
                float padding[2];
            } params;
            params.noise_scale = glm::vec2(width / 4.0f, height / 4.0f);
            params.radius = config.radius;
            params.bias = config.bias;
            params.kernel_size = config.kernel_size;
            params.power = config.power;
            memcpy(mapped, &params, sizeof(params));
            params_ubo->unmap();
        }
    }

    // SSAO Pass
    {
        rhi::RenderingInfo info{};
        info.render_area = {0, 0, width, height};
        rhi::RenderingAttachment ssao_att = {color_view.get(), nullptr, rhi::LoadOp::Clear,
                                             rhi::StoreOp::Store, rhi::ClearColor::white()};
        info.color_attachments = {&ssao_att, 1};

        cmd.begin_rendering(info);
        cmd.bind_pipeline(*pipeline);

        cmd.bind_descriptor_set(*pipeline_layout, 0, camera_set);
        cmd.bind_descriptor_set(*pipeline_layout, 1, gbuffer_set);
        cmd.bind_descriptor_set(*pipeline_layout, 2, *descriptor_set);

        cmd.bind_vertex_buffer(0, quad_vb);
        cmd.draw(6, 1, 0, 0);

        cmd.end_rendering();
    }

    // Blur Pass
    {
        rhi::TextureBarrier barrier{};
        barrier.texture = color_texture.get();
        barrier.old_state = rhi::ResourceState::RenderTarget;
        barrier.new_state = rhi::ResourceState::ShaderResource;
        cmd.barrier(barrier);

        rhi::RenderingInfo blur_info{};
        blur_info.render_area = {0, 0, width, height};
        std::vector<rhi::RenderingAttachment> attachments;
        attachments.push_back({blur_view.get(), nullptr, rhi::LoadOp::Clear, rhi::StoreOp::Store,
                               rhi::ClearColor{0.0f, 0.0f, 0.0f, 0.0f}});
        blur_info.color_attachments = attachments;

        cmd.begin_rendering(blur_info);
        cmd.bind_pipeline(*blur_pipeline);

        cmd.bind_descriptor_set(*blur_layout, 0, *blur_descriptor_set);
        cmd.bind_vertex_buffer(0, quad_vb);
        cmd.draw(6, 1, 0, 0);
        cmd.end_rendering();

        rhi::TextureBarrier barrier2{};
        barrier2.texture = blur_texture.get();
        barrier2.old_state = rhi::ResourceState::RenderTarget;
        barrier2.new_state = rhi::ResourceState::ShaderResource;
        cmd.barrier(barrier2);
    }
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
    update_composite_descriptor_set();

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

    if (m_lighting_data_set && m_light_ubo) {
        m_lighting_data_set->write_buffer(0, *m_light_ubo);
        if (m_point_light_ssbo) {
            m_lighting_data_set->write_storage_buffer(1, *m_point_light_ssbo);
        }
        if (m_shadow_ubo) {
            m_lighting_data_set->write_buffer(2, *m_shadow_ubo);
        }
    }

    m_ssao.create(m_device, m_width, m_height, m_ssao.config, *m_camera_layout,
                  *m_gbuffer_input_layout);
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
    m_ssao.destroy();
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
    update_composite_descriptor_set();

    m_ssr.destroy();
    m_ssr.create(m_device, m_width / 2, m_height / 2, m_ssr.config);

    m_ssao.destroy();
    m_ssao.create(m_device, m_width, m_height, m_ssao.config, *m_camera_layout,
                  *m_gbuffer_input_layout);
    update_gbuffer_descriptor_set();

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

    m_csm.update_cascades(camera, glm::vec3(0.0f, -1.0f, 0.0f),
                          aspect); // Update shadow cascades (placeholder light dir)
}

void DeferredRenderer::execute_ssao_pass(rhi::CommandList& cmd, const Camera& camera) {
    if (!m_ssao.config.enabled || !m_quad_vb || !m_camera_set || !m_gbuffer_input_set)
        return;
    m_ssao.execute(cmd, m_device, camera, *m_quad_vb, *m_camera_set, *m_gbuffer_input_set);
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

void DeferredRenderer::update_csm(const Camera& camera, const glm::vec3& light_dir) {
    float aspect_ratio =
        m_height == 0 ? 1.0f : static_cast<float>(m_width) / static_cast<float>(m_height);
    m_csm.update_cascades(camera, light_dir, aspect_ratio);
}

void DeferredRenderer::begin_shadow_pass(rhi::CommandList& cmd, u32 cascade_index) {
    if (cascade_index >= m_csm.config.cascade_count)
        return;

    rhi::RenderingInfo render_info{};
    render_info.render_area = {0, 0, m_csm.config.resolution, m_csm.config.resolution};
    render_info.layer_count = 1;

    rhi::RenderingAttachment depth_att{};
    depth_att.view = m_csm.cascade_views[cascade_index].get();
    depth_att.load_op = rhi::LoadOp::Clear;
    depth_att.store_op = rhi::StoreOp::Store;
    depth_att.clear_value = rhi::ClearDepthStencil{1.0f, 0};

    render_info.depth_attachment = &depth_att;

    // Transition depth layer to DepthWrite
    rhi::TextureBarrier b;
    b.texture = m_csm.depth_array_texture.get();
    b.old_state = rhi::ResourceState::Undefined;
    b.new_state = rhi::ResourceState::DepthWrite;
    b.base_array_layer = cascade_index;
    b.array_layer_count = 1;
    cmd.barrier(b);

    cmd.begin_rendering(render_info);

    cmd.bind_pipeline(*m_shadow_pipeline);
    cmd.set_viewport_and_scissor({m_csm.config.resolution, m_csm.config.resolution});
    cmd.set_depth_bias(1.25f, 0.0f, 1.75f);
}

void DeferredRenderer::end_shadow_pass(rhi::CommandList& cmd) {
    cmd.end_rendering();
}

u32 DeferredRenderer::get_shadow_cascade_count() const {
    return m_csm.config.cascade_count;
}

glm::mat4 DeferredRenderer::get_shadow_view_projection(u32 cascade_index) const {
    if (cascade_index >= m_csm.config.cascade_count)
        return glm::mat4(1.0f);
    return m_csm.cascades[cascade_index].view_projection;
}

const rhi::PipelineLayout* DeferredRenderer::get_shadow_layout() const {
    return m_shadow_layout.get();
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

    if (m_shadow_ubo) {
        DeferredShadowUBO shadow_data;
        for (u32 i = 0; i < 4; ++i) {
            shadow_data.light_space_matrices[i] = m_csm.cascades[i].view_projection;
        }
        shadow_data.cascade_splits =
            glm::vec4(m_csm.cascades[0].split_depth, m_csm.cascades[1].split_depth,
                      m_csm.cascades[2].split_depth, m_csm.cascades[3].split_depth);
        shadow_data.params = glm::vec4(1.0f, 0.005f, 0.0f, 0.0f); // Enabled, Bias
        m_shadow_ubo->upload(shadow_data);
        m_shadow_ubo->upload(shadow_data);
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

    // Bind Descriptor Set
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
void DeferredRenderer::set_ssao_config(const SSAOConfig& config) {
    m_ssao.config = config;
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

    // GBuffer input layout (set 1: GBuffer textures + Shadow Map)
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
        desc.bindings.push_back(
            rhi::DescriptorBinding::combined_image_sampler(4, rhi::ShaderStage::Fragment));
        desc.bindings.push_back(
            rhi::DescriptorBinding::combined_image_sampler(5, rhi::ShaderStage::Fragment));
        m_gbuffer_input_layout = m_device.create_descriptor_set_layout(desc);
    }

    // Lighting data layout (set 2: LightUBO + PointLightSSBO + ShadowUBO)
    {
        rhi::DescriptorSetLayoutDesc desc;
        desc.bindings.push_back(
            rhi::DescriptorBinding::uniform_buffer(0, rhi::ShaderStage::Fragment));
        desc.bindings.push_back(
            rhi::DescriptorBinding::storage_buffer(1, rhi::ShaderStage::Fragment));
        desc.bindings.push_back(
            rhi::DescriptorBinding::uniform_buffer(2, rhi::ShaderStage::Fragment));
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

    // Geometry pipeline layout: set 0 (camera), set 1 (material), push constants (model + material)
    {
        rhi::PipelineLayoutDesc desc;
        desc.set_layouts.push_back(m_camera_layout.get());
        desc.set_layouts.push_back(m_material_layout.get());

        // Vertex: Model Matrix (64 bytes)
        rhi::PushConstantRange pc_vert;
        pc_vert.stages = rhi::ShaderStage::Vertex;
        pc_vert.offset = 0;
        pc_vert.size = sizeof(glm::mat4);
        desc.push_constant_ranges.push_back(pc_vert);

        // Fragment: Material Params (offset 64, size 24)
        // vec4 albedo + float roughness + float metallic
        rhi::PushConstantRange pc_frag;
        pc_frag.stages = rhi::ShaderStage::Fragment;
        pc_frag.offset = 64;
        pc_frag.size = sizeof(glm::vec4) + sizeof(f32) * 2;
        desc.push_constant_ranges.push_back(pc_frag);

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

    // Shadow pipeline layout: push constants (MVP)
    {
        rhi::PipelineLayoutDesc desc;
        rhi::PushConstantRange pc_range;
        pc_range.stages = rhi::ShaderStage::Vertex;
        pc_range.offset = 0;
        pc_range.size = sizeof(glm::mat4);
        desc.push_constant_ranges.push_back(pc_range);
        m_shadow_layout = m_device.create_pipeline_layout(desc);
    }

    // =========================================================================
    // Render Passes (for compatibility)
    // =========================================================================
    m_geometry_pass = m_device.create_render_pass(rhi::RenderPassDesc::gbuffer());
    m_lighting_pass =
        m_device.create_render_pass(rhi::RenderPassDesc::simple(rhi::Format::RGBA16_FLOAT));
    m_composite_pass =
        m_device.create_render_pass(rhi::RenderPassDesc::simple(m_swapchain.format()));
    m_shadow_pass =
        m_device.create_render_pass(rhi::RenderPassDesc::shadow_map(rhi::Format::D32_FLOAT));

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
    auto vk_shadow_vert = m_device.create_shader_from_file("assets/shaders/deferred/vk_shadow.vert",
                                                           rhi::ShaderStage::Vertex, "ShadowVert");

    if (!vk_geometry_vert || !vk_geometry_frag || !vk_fullscreen_vert || !vk_lighting_frag ||
        !vk_composite_frag || !vk_shadow_vert) {
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
        desc.render_pass = m_geometry_pass.get();
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
        desc.render_pass = m_lighting_pass.get();
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
        desc.render_pass = m_composite_pass.get();
        m_composite_pipeline = m_device.create_graphics_pipeline(desc);
    }

    // =========================================================================
    // Shadow Pipeline
    // =========================================================================
    {
        rhi::GraphicsPipelineDesc desc;
        desc.vertex_shader = vk_shadow_vert.get();
        desc.vertex_layout = rhi::VertexInputLayout::standard_vertex();
        desc.topology = rhi::PrimitiveTopology::TriangleList;
        desc.rasterization = rhi::RasterizationState::shadow_map();
        desc.depth_stencil = rhi::DepthStencilState::default_state();
        desc.blend = rhi::BlendState::disabled(0);
        desc.multisample = {};
        desc.layout = m_shadow_layout.get();
        desc.render_pass = m_shadow_pass.get();
        m_shadow_pipeline = m_device.create_graphics_pipeline(desc);
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
    m_camera_ubo =
        m_device.create_uniform_buffer(sizeof(glm::mat4) * 4 + sizeof(glm::vec4), "CameraUBO");
    m_light_ubo =
        m_device.create_uniform_buffer(sizeof(glm::vec4) * 2 + sizeof(glm::uvec4), "LightUBO");
    m_shadow_ubo = m_device.create_uniform_buffer(sizeof(DeferredShadowUBO), "ShadowUBO");

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

    {
        rhi::SamplerDesc desc = rhi::SamplerDesc::shadow();
        desc.debug_name = "ShadowSampler";
        m_shadow_sampler = m_device.create_sampler(desc);
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

    if (m_csm.depth_array_view && m_shadow_sampler) {
        m_gbuffer_input_set->write_texture(4, *m_csm.depth_array_view, *m_shadow_sampler);
    }

    if (m_ssao.blur_view) {
        m_gbuffer_input_set->write_texture(5, *m_ssao.blur_view, *m_nearest_sampler);
    }
}

void DeferredRenderer::update_composite_descriptor_set() {
    if (!m_composite_input_set || !m_nearest_sampler || !m_lighting_view) {
        return;
    }

    m_composite_input_set->write_texture(0, *m_lighting_view, *m_nearest_sampler);
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

[[nodiscard]] std::unique_ptr<rhi::DescriptorSet> DeferredRenderer::create_material_descriptor_set(
    const rhi::TextureView& albedo, const rhi::TextureView& normal, const rhi::TextureView& arm) {

    auto set = m_descriptor_pool->allocate(*m_material_layout);

    std::vector<rhi::DescriptorWrite> writes;
    writes.push_back(rhi::DescriptorWrite::combined_image_sampler(0, albedo, *m_linear_sampler));
    writes.push_back(rhi::DescriptorWrite::combined_image_sampler(1, normal, *m_linear_sampler));
    writes.push_back(rhi::DescriptorWrite::combined_image_sampler(2, arm, *m_linear_sampler));

    set->write(writes);
    return set;
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
