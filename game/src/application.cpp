/**
 * @file application.cpp
 * @brief Main application implementation
 */

#include "application.hpp"

#include <fstream>
#include <sstream>

#include <GLFW/glfw3.h>
#include <engine/core/log.hpp>
#include <engine/core/memory.hpp>
#include <engine/renderer/camera.hpp>

// RHI Includes
#include <engine/rhi/vulkan/vk_device.hpp>
#include <engine/rhi/vulkan/vk_swapchain.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>

namespace game {

std::string Application::read_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        HZ_ERROR("Failed to open file: {}", path);
        return "";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

bool Application::init() {
    hz::Log::init();
    hz::MemoryContext::init();

    if (!init_window()) {
        return false;
    }

    if (!init_renderer()) {
        return false;
    }

    init_input();
    init_scene();
    load_assets();
    setup_scene_entities();

    HZ_LOG_INFO("Application initialized successfully");
    return true;
}

bool Application::init_window() {
    hz::WindowConfig config;
    config.title = "Horizon Engine - Deferred PBR Test";
    config.width = GameConfig::WINDOW_WIDTH;
    config.height = GameConfig::WINDOW_HEIGHT;
    config.vsync = false;

    // Window needs to be created before RHI so we can get surface
    m_window = std::make_unique<hz::Window>(config);

    m_imgui = std::make_unique<hz::ImGuiLayer>();
    m_imgui->init(*m_window);

    return true;
}

bool Application::init_renderer() {
    // 1. Initialize RHI Device (Vulkan)
    hz::rhi::DeviceDesc device_desc;
    device_desc.application_name = "Horizon Engine";
    device_desc.enable_validation = true; // Enable validation for dev
    m_device = std::make_unique<hz::rhi::vk::VulkanDevice>(device_desc);

    // 2. Initialize Swapchain
    hz::rhi::SwapchainDesc swapchain_desc;
    swapchain_desc.width = GameConfig::WINDOW_WIDTH;
    swapchain_desc.height = GameConfig::WINDOW_HEIGHT;
    swapchain_desc.vsync = false;
    swapchain_desc.window_handle = m_window->native_handle();
    m_swapchain = std::make_unique<hz::rhi::vk::VulkanSwapchain>(
        *static_cast<hz::rhi::vk::VulkanDevice*>(m_device.get()), swapchain_desc);

    // 3. Initialize Deferred Renderer
    m_renderer = std::make_unique<hz::DeferredRenderer>(*m_device, *m_swapchain);
    if (!m_renderer->init()) {
        HZ_FATAL("Failed to initialize Deferred Renderer");
        return false;
    }

    m_debug_renderer = std::make_unique<hz::DebugRenderer>();

    // 4. Initialize Synchronization primitives
    for (unsigned int i = 0; i < APP_MAX_FRAMES_IN_FLIGHT; ++i) {
        m_image_available_sems[i] = m_device->create_semaphore();
        m_frame_fences[i] = m_device->create_fence(true); // Signaled initially
    }
    m_render_finished_sems.clear();
    m_render_finished_sems.reserve(m_swapchain->image_count());
    for (hz::u32 i = 0; i < m_swapchain->image_count(); ++i) {
        m_render_finished_sems.push_back(m_device->create_semaphore());
    }

    return true;
}

void Application::init_input() {
    m_input = std::make_unique<hz::InputManager>();
    m_input->attach(*m_window);
    m_input->bind_key(hz::InputManager::ACTION_MOVE_FORWARD, GLFW_KEY_W);
    m_input->bind_key(hz::InputManager::ACTION_MOVE_BACKWARD, GLFW_KEY_S);
    m_input->bind_key(hz::InputManager::ACTION_MOVE_LEFT, GLFW_KEY_A);
    m_input->bind_key(hz::InputManager::ACTION_MOVE_RIGHT, GLFW_KEY_D);
    m_input->bind_key(hz::InputManager::ACTION_JUMP, GLFW_KEY_SPACE);
    m_input->bind_key(hz::InputManager::ACTION_CROUCH, GLFW_KEY_LEFT_CONTROL);
    m_input->bind_key(hz::InputManager::ACTION_SPRINT, GLFW_KEY_LEFT_SHIFT);
    m_input->bind_key(hz::InputManager::ACTION_MENU, GLFW_KEY_ESCAPE);
    m_input->bind_mouse_button(hz::InputManager::ACTION_PRIMARY_FIRE, GLFW_MOUSE_BUTTON_LEFT);
}

void Application::init_scene() {
    m_scene = std::make_unique<hz::Scene>();
    m_physics = std::make_unique<hz::PhysicsWorld>();
    m_audio = std::make_unique<hz::AudioSystem>();

    m_physics_system.init(*m_physics);
    m_audio->init();
}

void Application::load_assets() {
    m_assets = std::make_unique<hz::AssetRegistry>();

    // 1. Create Meshes & Models
    {
        auto mesh = hz::Mesh::create_plane(50.0f, 10.0f);
        mesh.upload_to_gpu(*m_device, "PlaneMesh");
        hz::Model model;
        model.add_mesh(std::move(mesh));
        m_plane_handle = m_assets->register_model(std::move(model), "plane");
    }
    {
        auto mesh = hz::Mesh::create_sphere(1.0f, 64, 64);
        mesh.upload_to_gpu(*m_device, "SphereMesh");
        hz::Model model;
        model.add_mesh(std::move(mesh));
        m_sphere_handle = m_assets->register_model(std::move(model), "sphere");
    }
    {
        auto mesh = hz::Mesh::create_cube(1.0f);
        mesh.upload_to_gpu(*m_device, "CubeMesh");
        hz::Model model;
        model.add_mesh(std::move(mesh));
        m_cube_handle = m_assets->register_model(std::move(model), "cube");
    }

    // 2. Load/Create Default Textures
    // Albedo (White)
    {
        hz::u8 data[4] = {200, 200, 200, 255};
        hz::Texture tex = hz::Texture::create(1, 1, hz::TextureFormat::RGBA8, data);
        tex.upload_to_gpu(*m_device);
        m_albedo_handle = m_assets->register_texture(std::move(tex), "default_albedo");
    }

    // Normal (Flat Z+)
    {
        hz::u8 data[4] = {128, 128, 255, 255};
        hz::Texture tex = hz::Texture::create(1, 1, hz::TextureFormat::RGBA8, data);
        tex.upload_to_gpu(*m_device);
        m_normal_handle = m_assets->register_texture(std::move(tex), "default_normal");
    }

    // ARM (AO=1, Roughness=0.8, Metallic=0.0) -> Plastic-like
    {
        hz::u8 data[4] = {255, 204, 0, 255};
        hz::Texture tex = hz::Texture::create(1, 1, hz::TextureFormat::RGBA8, data);
        tex.upload_to_gpu(*m_device);
        m_arm_handle = m_assets->register_texture(std::move(tex), "default_arm");
    }

    // Initialize IBL
    m_ibl = std::make_unique<hz::IBL>();
    bool ibl_ready = false;

    if (ibl_ready) {
        HZ_LOG_INFO("IBL initialized!");
    } else {
        HZ_LOG_WARN("IBL initialization temporarily disabled during refactor");
    }

    // Create default material set
    if (m_albedo_handle.is_valid() && m_normal_handle.is_valid() && m_arm_handle.is_valid()) {
        auto* albedo = m_assets->get_texture(m_albedo_handle);
        auto* normal = m_assets->get_texture(m_normal_handle);
        auto* arm = m_assets->get_texture(m_arm_handle);

        if (albedo && normal && arm) {
            m_default_material_set = m_renderer->create_material_descriptor_set(
                *albedo->rhi_view(), *normal->rhi_view(), *arm->rhi_view());
        }
    } else {
        HZ_ERROR("Failed to create default material descriptor set: Textures not ready");
    }
}

void Application::setup_scene_entities() {
    // Player
    auto player_entity = m_scene->create_entity();
    {
        auto& tc = m_scene->registry().emplace<hz::TransformComponent>(player_entity);
        tc.position = glm::vec3(0.0f, 2.0f, 6.0f);
        tc.rotation = glm::vec3(-10.0f, 0.0f, 0.0f);
        auto& cc = m_scene->registry().emplace<hz::CameraComponent>(player_entity);
        cc.primary = true;
    }

    // Ground Plane
    if (m_plane_handle.is_valid()) {
        auto plane = m_scene->create_entity();
        auto& tc = m_scene->registry().emplace<hz::TransformComponent>(plane);
        tc.position = glm::vec3(0.0f, -1.0f, 0.0f);
        tc.scale = glm::vec3(1.0f);

        auto& mc = m_scene->registry().emplace<hz::MeshComponent>(plane);
        mc.mesh_type = hz::MeshComponent::MeshType::Model;
        mc.model = m_plane_handle;
        mc.metallic = 0.1f;
        mc.roughness = 0.8f;
        mc.albedo_color = glm::vec3(0.8f);

        // Physics
        auto& rb = m_scene->registry().emplace<hz::RigidBodyComponent>(plane);
        rb.type = hz::RigidBodyComponent::BodyType::Static;

        auto& bc = m_scene->registry().emplace<hz::BoxColliderComponent>(plane);
        bc.half_extents = glm::vec3(25.0f, 0.1f, 25.0f);
    }

    // Test Cube
    if (m_cube_handle.is_valid()) {
        auto cube = m_scene->create_entity();
        auto& tc = m_scene->registry().emplace<hz::TransformComponent>(cube);
        tc.position = glm::vec3(-2.0f, 10.0f, 0.0f); // Higher up to fall

        auto& mc = m_scene->registry().emplace<hz::MeshComponent>(cube);
        mc.mesh_type = hz::MeshComponent::MeshType::Model;
        mc.model = m_cube_handle;
        mc.albedo_color = glm::vec3(1.0f, 0.2f, 0.2f); // Reddish
        mc.metallic = 0.9f;
        mc.roughness = 0.1f;

        // Physics
        auto& rb = m_scene->registry().emplace<hz::RigidBodyComponent>(cube);
        rb.type = hz::RigidBodyComponent::BodyType::Dynamic;
        rb.mass = 10.0f;

        auto& bc = m_scene->registry().emplace<hz::BoxColliderComponent>(cube);
        bc.half_extents = glm::vec3(0.5f);
    }

    // Test Sphere
    if (m_sphere_handle.is_valid()) {
        auto sphere = m_scene->create_entity();
        auto& tc = m_scene->registry().emplace<hz::TransformComponent>(sphere);
        tc.position = glm::vec3(2.0f, 10.0f, 0.0f); // Higher up

        auto& mc = m_scene->registry().emplace<hz::MeshComponent>(sphere);
        mc.mesh_type = hz::MeshComponent::MeshType::Model;
        mc.model = m_sphere_handle;
        mc.albedo_color = glm::vec3(0.2f, 1.0f, 0.2f); // Greenish
        mc.metallic = 0.0f;
        mc.roughness = 0.2f;

        // Physics
        auto& rb = m_scene->registry().emplace<hz::RigidBodyComponent>(sphere);
        rb.type = hz::RigidBodyComponent::BodyType::Dynamic;
        rb.mass = 5.0f;

        auto& sc = m_scene->registry().emplace<hz::SphereColliderComponent>(sphere);
        sc.radius = 1.0f;
    }

    // Point Light
    auto light = m_scene->create_entity();
    {
        auto& tc = m_scene->registry().emplace<hz::TransformComponent>(light);
        tc.position = glm::vec3(0.0f, 3.0f, 2.0f);

        auto& lc = m_scene->registry().emplace<hz::LightComponent>(light);
        lc.type = hz::LightType::Point;
        lc.color = glm::vec3(1.0f, 0.8f, 0.5f);
        lc.intensity = 10.0f;
        lc.range = 20.0f;
    }
}

void Application::run() {
    hz::GameLoop loop;
    loop.set_update_callback([this](hz::f64 dt) { on_update(static_cast<float>(dt)); });
    loop.set_render_callback([this](hz::f64 alpha) { on_render(static_cast<float>(alpha)); });
    HZ_LOG_INFO("Starting game loop...");
    loop.run();
}

void Application::on_update(float dt) {
    m_lifetime_system.update(*m_scene, dt);
    m_physics_system.update(*m_scene, *m_physics, dt);

    // Find player camera for character system
    glm::vec3 cam_pos(0.0f);
    glm::vec3 cam_rot(0.0f);
    auto view = m_scene->registry().view<hz::TransformComponent, hz::CameraComponent>();
    for (auto entity : view) {
        auto& cc = view.get<hz::CameraComponent>(entity);
        if (cc.primary) {
            auto& tc = view.get<hz::TransformComponent>(entity);
            cam_pos = tc.position;
            cam_rot = tc.rotation;
            break;
        }
    }
    m_character_system.update(*m_scene, cam_pos, cam_rot);

    m_player_system.update(*m_scene, *m_input, *m_window, dt);
    m_animation_system.update(*m_scene, dt);

    if (m_input->is_action_just_pressed(hz::InputManager::ACTION_MENU)) {
        m_window->close();
    }
    m_input->update();
}

void Application::on_render([[maybe_unused]] float alpha) {
    // 0. Wait for previous frame work to finish
    m_current_frame = m_device->begin_frame();

    hz::rhi::Fence* fences[] = {m_frame_fences[m_current_frame].get()};
    m_device->wait_fences(fences);

    // Camera & Light setup
    std::vector<hz::GPUPointLight> point_lights;
    std::vector<hz::GPUSpotLight> spot_lights;
    glm::vec3 sun_dir = glm::normalize(glm::vec3(-0.5f, -1.0f, -0.3f));
    glm::vec3 sun_color = glm::vec3(1.0f, 0.9f, 0.8f);

    hz::Camera camera;
    {
        auto view = m_scene->registry().view<hz::TransformComponent, hz::CameraComponent>();
        for (auto entity : view) {
            auto& cc = view.get<hz::CameraComponent>(entity);
            if (cc.primary) {
                auto& tc = view.get<hz::TransformComponent>(entity);
                camera = hz::Camera(tc.position, glm::vec3(0.0f, 1.0f, 0.0f), tc.rotation.y,
                                    tc.rotation.x);
                camera.fov = cc.fov;
                break;
            }
        }
    }

    // 1. Acquire Image
    if (!m_swapchain->acquire_next_image(m_image_available_sems[m_current_frame].get())) {
        // Handle resize (swapchain out of date)
        auto [width, height] = m_window->framebuffer_size();
        if (width > 0 && height > 0) {
            m_device->wait_idle();
            m_swapchain->resize(width, height);
            m_renderer->resize(width, height);
            m_render_finished_sems.clear();
            m_render_finished_sems.reserve(m_swapchain->image_count());
            for (hz::u32 i = 0; i < m_swapchain->image_count(); ++i) {
                m_render_finished_sems.push_back(m_device->create_semaphore());
            }
        }
        m_device->end_frame();
        return;
    }

    const hz::u32 image_index = m_swapchain->current_image_index();
    if (image_index >= m_render_finished_sems.size() || !m_render_finished_sems[image_index]) {
        HZ_LOG_ERROR("Invalid swapchain image index for render semaphore: {}", image_index);
        m_device->end_frame();
        return;
    }

    // 2. Get Command List
    auto cmd = m_device->create_command_list(hz::rhi::QueueType::Graphics);
    if (!cmd) {
        m_device->end_frame();
        return;
    }

    cmd->begin();

    // Shadow setup logic skipped

    // === Shadow Pass ===
    m_renderer->update_csm(camera, sun_dir);

    for (hz::u32 i = 0; i < m_renderer->get_shadow_cascade_count(); ++i) {
        m_renderer->begin_shadow_pass(*cmd, i);
        glm::mat4 light_vp = m_renderer->get_shadow_view_projection(i);

        auto group =
            m_scene->registry().group<hz::TransformComponent>(entt::get<hz::MeshComponent>);
        for (auto entity : group) {
            auto [tc, mc] = group.get<hz::TransformComponent, hz::MeshComponent>(entity);
            if (!mc.cast_shadows)
                continue;

            const hz::Mesh* mesh = resolve_mesh(mc);
            if (mesh) {
                glm::mat4 model = tc.get_transform();
                glm::mat4 mvp = light_vp * model;
                cmd->push_constants(*m_renderer->get_shadow_layout(), hz::rhi::ShaderStage::Vertex,
                                    mvp);
                mesh->draw(*cmd);
            }
        }
        m_renderer->end_shadow_pass(*cmd);
    }

    // === Geometry Pass ===
    m_renderer->begin_geometry_pass(*cmd, camera);

    // Bind default material set (Set 1)
    if (m_default_material_set) {
        cmd->bind_descriptor_set(*m_renderer->get_geometry_layout(), 1, *m_default_material_set);
    }

    // Render entities
    {
        auto group =
            m_scene->registry().group<hz::TransformComponent>(entt::get<hz::MeshComponent>);
        for (auto entity : group) {
            auto [tc, mc] = group.get<hz::TransformComponent, hz::MeshComponent>(entity);

            const hz::Mesh* mesh = resolve_mesh(mc);
            if (mesh) {
                // Push Model Matrix (Vertex Stage, Offset 0)
                glm::mat4 model = tc.get_transform();
                cmd->push_constants(*m_renderer->get_geometry_layout(),
                                    hz::rhi::ShaderStage::Vertex, model, 0);

                // Push Material Params (Fragment Stage, Offset 64)
                struct MatParams {
                    glm::vec4 albedo;
                    float roughness;
                    float metallic;
                } mat_params;
                mat_params.albedo = glm::vec4(mc.albedo_color, 1.0f);
                mat_params.roughness = mc.roughness;
                mat_params.metallic = mc.metallic;

                cmd->push_constants(*m_renderer->get_geometry_layout(),
                                    hz::rhi::ShaderStage::Fragment, mat_params, 64);

                // Draw
                mesh->draw(*cmd);
            }
        }
    }

    m_renderer->end_geometry_pass(*cmd);

    // === SSAO Pass ===
    m_renderer->execute_ssao_pass(*cmd, camera);

    // === Lighting Pass ===
    m_renderer->execute_lighting_pass(*cmd, camera, point_lights, spot_lights, sun_dir, sun_color);

    // === TAA & Post ===
    m_renderer->execute_taa_pass(*cmd);
    m_renderer->execute_post_process(*cmd, camera, 1.0f, 1.0f, 1.0f);

    // === Render to Screen/Swapchain ===
    m_renderer->render_to_screen(*cmd);

    // === UI ===
    m_imgui->begin_frame();
    ImGui::Begin("PBR Test (Vulkan)");
    ImGui::Text("Profiling: %.2f ms", 1000.0f / ImGui::GetIO().Framerate);
    ImGui::End();
    m_imgui->end_frame();

    cmd->end();

    // 3. Submit
    hz::rhi::SubmitInfo submit_info{};
    hz::rhi::CommandList* cmd_ptr = cmd.get();
    submit_info.command_lists = {&cmd_ptr, 1};

    hz::rhi::Semaphore* wait_sems[] = {m_image_available_sems[m_current_frame].get()};
    hz::rhi::Semaphore* signal_sems[] = {m_render_finished_sems[image_index].get()};

    submit_info.wait_semaphores = wait_sems;
    submit_info.signal_semaphores = signal_sems;
    submit_info.signal_fence = m_frame_fences[m_current_frame].get();

    m_device->reset_fences(fences);
    m_device->submit(hz::rhi::QueueType::Graphics, {&submit_info, 1});

    // 4. Present
    hz::rhi::Semaphore* present_wait_sems[] = {m_render_finished_sems[image_index].get()};
    m_swapchain->present(present_wait_sems);

    m_device->end_frame();

    glfwPollEvents();
}

void Application::shutdown() {
    m_device->wait_idle();
    m_renderer->shutdown();
    m_swapchain.reset();
    m_device.reset();

    m_imgui->shutdown();
    m_physics->shutdown();
    m_audio->shutdown();
    hz::MemoryContext::shutdown();
    hz::Log::shutdown();
}

const hz::Mesh* Application::resolve_mesh(const hz::MeshComponent& mc) {
    // Use cached pointer if available
    if (mc.mesh_render_ptr) {
        return mc.mesh_render_ptr;
    }

    hz::ModelHandle handle;

    if (mc.mesh_type == hz::MeshComponent::MeshType::Model) {
        handle = mc.model;
    } else {
        // Primitive: map name to the pre-registered model handle
        const std::string& name = mc.primitive_name.empty() ? mc.mesh_path : mc.primitive_name;
        if (name == "cube") {
            handle = m_cube_handle;
        } else if (name == "sphere") {
            handle = m_sphere_handle;
        } else if (name == "plane") {
            handle = m_plane_handle;
        }
    }

    if (!handle.is_valid()) {
        return nullptr;
    }

    if (auto* model = m_assets->get_model(handle)) {
        if (!model->meshes().empty()) {
            return &model->meshes()[0];
        }
    }

    return nullptr;
}

} // namespace game
