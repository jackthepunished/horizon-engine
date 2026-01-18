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
#include <engine/renderer/opengl/gl_context.hpp>
#include <glad/glad.h>
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

    m_window = std::make_unique<hz::Window>(config);

    if (!hz::gl::init_context()) {
        HZ_FATAL("Failed to initialize OpenGL context");
        return false;
    }

    m_imgui = std::make_unique<hz::ImGuiLayer>();
    m_imgui->init(*m_window);

    return true;
}

bool Application::init_renderer() {
    m_renderer = std::make_unique<hz::DeferredRenderer>();
    if (!m_renderer->init(GameConfig::WINDOW_WIDTH, GameConfig::WINDOW_HEIGHT)) {
        HZ_FATAL("Failed to initialize Deferred Renderer");
        return false;
    }

    m_debug_renderer = std::make_unique<hz::DebugRenderer>();
    m_debug_renderer->init();

    // Load shaders
    m_geometry_shader =
        std::make_unique<hz::gl::Shader>(read_file("assets/shaders/deferred/geometry.vert"),
                                         read_file("assets/shaders/deferred/geometry.frag"));

    m_shadow_shader =
        std::make_unique<hz::gl::Shader>(read_file("assets/shaders/deferred/shadow.vert"),
                                         read_file("assets/shaders/deferred/shadow.frag"));

    // Configure geometry shader samplers
    m_geometry_shader->bind();
    m_geometry_shader->set_int("u_AlbedoMap", 0);
    m_geometry_shader->set_int("u_NormalMap", 1);
    m_geometry_shader->set_int("u_MetallicRoughnessMap", 2);
    m_geometry_shader->set_int("u_AOMap", 3);
    m_geometry_shader->set_int("u_EmissionMap", 4);

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

    // Create primitive meshes
    m_sphere_mesh = hz::Mesh::create_sphere(1.0f);
    m_cube_mesh = hz::Mesh::create_cube(1.0f);
}

void Application::load_assets() {
    // Initialize IBL
    m_ibl = std::make_unique<hz::IBL>();
    bool ibl_ready =
        m_ibl->generate("assets/textures/skybox/afrikaans_church_interior_4k.hdr", 1024);

    if (ibl_ready) {
        HZ_LOG_INFO("IBL initialized with Afrikaans Church HDR!");
        m_irradiance_map = m_ibl->irradiance_map();
        m_prefilter_map = m_ibl->prefilter_map();
        m_brdf_lut = m_ibl->brdf_lut();
        m_environment_map = m_ibl->environment_map();
    } else {
        HZ_LOG_WARN("IBL initialization failed!");
    }

    // Load treasure chest model
    m_test_model = hz::Model::load_from_gltf("assets/models/treasure_chest/treasure_chest_4k.gltf");
    if (m_test_model && m_test_model->is_valid()) {
        HZ_LOG_INFO("Test model loaded! Mesh count: {}", m_test_model->mesh_count());

        // Load textures
        hz::TextureParams albedo_params;
        albedo_params.srgb = true;
        albedo_params.flip_y = false;
        albedo_params.generate_mipmaps = true;

        hz::TextureParams linear_params;
        linear_params.srgb = false;
        linear_params.flip_y = false;
        linear_params.generate_mipmaps = true;

        m_albedo_tex = hz::Texture::load_from_file(
            "assets/models/treasure_chest/textures/treasure_chest_diff_4k.jpg", albedo_params);
        m_normal_tex = hz::Texture::load_from_file(
            "assets/models/treasure_chest/textures/treasure_chest_nor_gl_4k.jpg", linear_params);
        m_arm_tex = hz::Texture::load_from_file(
            "assets/models/treasure_chest/textures/treasure_chest_arm_4k.jpg", linear_params);
    }

    // Load character model
    m_character_model = hz::Model::load_from_fbx("assets/models/character.fbx");
    if (m_character_model && m_character_model->is_valid()) {
        HZ_LOG_INFO("Character model loaded! Animations: {}",
                    m_character_model->animations().size());
        m_animation_system.init(*m_character_model);
    } else {
        HZ_ERROR("Failed to load character model!");
    }
}

void Application::setup_scene_entities() {
    // Create player camera entity
    auto player_entity = m_scene->create_entity();
    {
        auto& tc = m_scene->registry().emplace<hz::TransformComponent>(player_entity);
        tc.position = glm::vec3(0.0f, GameConfig::GROUND_LEVEL, 6.0f);
        tc.rotation = glm::vec3(-12.0f, -90.0f, 0.0f);

        auto& cc = m_scene->registry().emplace<hz::CameraComponent>(player_entity);
        cc.primary = true;

        auto& tag = m_scene->registry().emplace<hz::TagComponent>(player_entity);
        tag.tag = "Player";

        auto& rb = m_scene->registry().emplace<hz::RigidBodyComponent>(player_entity);
        rb.type = hz::RigidBodyComponent::BodyType::Dynamic;
        rb.mass = GameConfig::PLAYER_MASS;
        rb.fixed_rotation = true;

        auto& capsule = m_scene->registry().emplace<hz::CapsuleColliderComponent>(player_entity);
        capsule.radius = GameConfig::PLAYER_CAPSULE_RADIUS;
        capsule.half_height = GameConfig::PLAYER_CAPSULE_HALF_HEIGHT;
    }

    // Create PBR sphere grid
    for (int row = 0; row < GameConfig::PBR_GRID_ROWS; ++row) {
        float metallic = static_cast<float>(row) / static_cast<float>(GameConfig::PBR_GRID_ROWS);
        for (int col = 0; col < GameConfig::PBR_GRID_COLS; ++col) {
            float roughness =
                glm::clamp(static_cast<float>(col) / static_cast<float>(GameConfig::PBR_GRID_COLS),
                           0.25f, 1.0f);

            auto entity = m_scene->create_entity();

            auto& tc = m_scene->registry().emplace<hz::TransformComponent>(entity);
            tc.position = glm::vec3(
                (col - (GameConfig::PBR_GRID_COLS / 2)) * GameConfig::PBR_GRID_SPACING, 0.0f,
                (row - (GameConfig::PBR_GRID_ROWS / 2)) * GameConfig::PBR_GRID_SPACING);
            tc.scale = glm::vec3(1.0f);

            auto& mc = m_scene->registry().emplace<hz::MeshComponent>(entity);
            mc.mesh_type = hz::MeshComponent::MeshType::Primitive;
            mc.primitive_name = "sphere";
            mc.albedo_color = glm::vec3(1.0f, 0.0f, 0.0f);
            mc.metallic = metallic;
            mc.roughness = roughness;

            auto& tag = m_scene->registry().emplace<hz::TagComponent>(entity);
            tag.tag = "GridSphere";
        }
    }

    // Create treasure chest entity
    if (m_test_model && m_test_model->is_valid()) {
        auto entity = m_scene->create_entity();
        auto& tc = m_scene->registry().emplace<hz::TransformComponent>(entity);
        tc.position = glm::vec3(0.0f, 5.0f, 0.0f);
        tc.scale = glm::vec3(1.0f);

        auto& mc = m_scene->registry().emplace<hz::MeshComponent>(entity);
        mc.mesh_type = hz::MeshComponent::MeshType::Model;
        mc.model.index = 0;

        auto& tag = m_scene->registry().emplace<hz::TagComponent>(entity);
        tag.tag = "TreasureChest";

        auto& rb = m_scene->registry().emplace<hz::RigidBodyComponent>(entity);
        rb.type = hz::RigidBodyComponent::BodyType::Dynamic;
        rb.mass = 10.0f;

        auto& bc = m_scene->registry().emplace<hz::BoxColliderComponent>(entity);
        bc.half_extents = glm::vec3(1.0f);
    }

    // Create floor
    {
        auto entity = m_scene->create_entity();
        auto& tc = m_scene->registry().emplace<hz::TransformComponent>(entity);
        tc.position = glm::vec3(0.0f, -1.0f, 0.0f);
        tc.scale = glm::vec3(50.0f, 1.0f, 50.0f);

        auto& mc = m_scene->registry().emplace<hz::MeshComponent>(entity);
        mc.mesh_type = hz::MeshComponent::MeshType::Primitive;
        mc.primitive_name = "cube";
        mc.albedo_color = glm::vec3(0.5f);
        mc.metallic = 0.0f;
        mc.roughness = 0.8f;

        auto& tag = m_scene->registry().emplace<hz::TagComponent>(entity);
        tag.tag = "Floor";

        auto& rb = m_scene->registry().emplace<hz::RigidBodyComponent>(entity);
        rb.type = hz::RigidBodyComponent::BodyType::Static;

        auto& bc = m_scene->registry().emplace<hz::BoxColliderComponent>(entity);
        bc.half_extents = glm::vec3(50.0f, 1.0f, 50.0f);
    }

    // Create character entity
    if (m_character_model && m_character_model->is_valid()) {
        auto entity = m_scene->create_entity();
        auto& tc = m_scene->registry().emplace<hz::TransformComponent>(entity);
        tc.position = glm::vec3(5.0f, 0.0f, 0.0f);
        tc.scale = glm::vec3(1.0f);
        tc.rotation = glm::vec3(0.0f, 180.0f, 0.0f);

        auto& mc = m_scene->registry().emplace<hz::MeshComponent>(entity);
        mc.mesh_type = hz::MeshComponent::MeshType::Model;
        mc.model.index = 1;

        if (m_character_model->has_skeleton()) {
            auto& ac = m_scene->registry().emplace<hz::AnimatorComponent>(entity);
            ac.skeleton = m_character_model->skeleton();
            if (!m_character_model->animations().empty()) {
                auto anim = m_character_model->animations().back();
                ac.play(anim, true);
            }
        }
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
    // Physics
    m_physics_system.update(*m_scene, *m_physics, dt);

    // Player input & movement
    m_player_system.update(*m_scene, *m_input, *m_window, dt);
    m_player_system.handle_shooting(*m_scene, *m_input, *m_physics);

    // Sync character with camera
    glm::vec3 cam_pos = m_player_system.get_camera_position(*m_scene);
    glm::vec3 cam_rot = m_player_system.get_camera_rotation(*m_scene);
    m_character_system.update(*m_scene, cam_pos, cam_rot);

    // Animation
    m_animation_system.sync_with_player_movement(*m_scene, m_player_system.state().is_moving);
    m_animation_system.update(*m_scene, dt);

    if (m_animation_system.is_ik_enabled() && m_character_model) {
        m_animation_system.apply_ik(*m_scene, *m_character_model, m_ik_target_position);
    }

    // VFX cleanup
    m_lifetime_system.update(*m_scene, dt);

    // Menu/close
    if (m_input->is_action_just_pressed(hz::InputManager::ACTION_MENU)) {
        m_window->close();
    }

    // Toggle cursor
    if (glfwGetKey(m_window->native_handle(), GLFW_KEY_TAB) == GLFW_PRESS) {
        if (!m_tab_held) {
            m_window->set_cursor_captured(!m_window->is_cursor_captured());
            m_tab_held = true;
        }
    } else {
        m_tab_held = false;
    }

    m_input->update();
}

void Application::on_render([[maybe_unused]] float alpha) {
    // Lights
    std::vector<hz::GPUPointLight> point_lights = {
        {{-10.0f, 10.0f, 10.0f, 15.0f}, {300.0f, 300.0f, 300.0f, 5.0f}},
        {{10.0f, 10.0f, 10.0f, 15.0f}, {300.0f, 300.0f, 300.0f, 5.0f}}};
    std::vector<hz::GPUSpotLight> spot_lights;

    // Sun direction
    glm::vec3 sun_dir = glm::normalize(glm::vec3(-0.5f, -1.0f, -0.3f));
    glm::vec3 light_dir = -glm::normalize(sun_dir);
    glm::vec3 center = glm::vec3(0.0f);

    // Shadow matrices
    float near_plane = 1.0f;
    float far_plane = 60.0f;
    float ortho_size = 25.0f;
    glm::mat4 light_projection =
        glm::ortho(-ortho_size, ortho_size, -ortho_size, ortho_size, near_plane, far_plane);
    glm::vec3 light_pos = center + light_dir * (far_plane * 0.5f);
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
    if (glm::abs(glm::dot(light_dir, up)) > 0.9f) {
        up = glm::vec3(1.0f, 0.0f, 0.0f);
    }
    glm::mat4 light_view = glm::lookAt(light_pos, center, up);
    glm::mat4 light_space = light_projection * light_view;

    // === Shadow Pass ===
    m_renderer->begin_shadow_pass(light_space);
    m_shadow_shader->bind();
    m_shadow_shader->set_mat4("u_LightSpaceMatrix", light_space);

    // Shadow: spheres
    if (m_show_grid && m_sphere_mesh) {
        auto view = m_scene->registry().view<hz::TransformComponent, hz::MeshComponent>();
        for (auto [entity, tc, mc] : view.each()) {
            if (mc.mesh_type == hz::MeshComponent::MeshType::Primitive &&
                mc.primitive_name == "sphere") {
                m_shadow_shader->set_mat4("u_Model", tc.get_transform());
                m_sphere_mesh->draw();
            }
        }
    }

    // Shadow: treasure chest
    if (m_show_model && m_test_model && m_test_model->is_valid()) {
        auto view = m_scene->registry().view<hz::TransformComponent, hz::MeshComponent>();
        for (auto [entity, tc, mc] : view.each()) {
            if (mc.mesh_type == hz::MeshComponent::MeshType::Model && mc.model.index == 0) {
                glDisable(GL_CULL_FACE);
                m_shadow_shader->set_mat4("u_Model", tc.get_transform());
                m_test_model->draw();
                glEnable(GL_CULL_FACE);
            }
        }
    }

    // Shadow: character
    if (m_character_model && m_character_model->is_valid()) {
        auto view = m_scene->registry()
                        .view<hz::TransformComponent, hz::MeshComponent, hz::AnimatorComponent>();
        for (auto [entity, tc, mc, ac] : view.each()) {
            m_shadow_shader->set_mat4("u_Model", tc.get_transform());
            m_shadow_shader->set_bool("u_HasAnimation", true);
            if (!ac.bone_transforms.empty()) {
                m_shadow_shader->set_mat4_array("u_BoneMatrices", ac.bone_transforms.data(),
                                                ac.bone_transforms.size());
            }
            m_character_model->draw();
            m_shadow_shader->set_bool("u_HasAnimation", false);
        }
    }
    m_renderer->end_shadow_pass();

    // === Find Camera ===
    hz::Camera camera;
    {
        auto view = m_scene->registry().view<hz::TransformComponent, hz::CameraComponent>();
        for (auto [entity, tc, cc] : view.each()) {
            if (cc.primary) {
                camera = hz::Camera(tc.position, glm::vec3(0.0f, 1.0f, 0.0f), tc.rotation.y,
                                    tc.rotation.x);
                camera.fov = cc.fov;
                camera.near_plane = cc.near_plane;
                camera.far_plane = cc.far_plane;
                break;
            }
        }
    }

    // === Geometry Pass ===
    m_renderer->begin_geometry_pass(camera);
    m_geometry_shader->bind();
    m_geometry_shader->set_mat4("u_View", camera.view_matrix());

    glm::mat4 proj = camera.projection_matrix(GameConfig::ASPECT_RATIO);
    glm::mat4 jittered_proj = m_renderer->get_taa_jittered_projection(proj);
    m_geometry_shader->set_mat4("u_Projection", jittered_proj);
    m_geometry_shader->set_mat4("u_PrevViewProjection", m_prev_view_projection);
    m_prev_view_projection = proj * camera.view_matrix();

    // Render primitives
    {
        auto view = m_scene->registry().view<hz::TransformComponent, hz::MeshComponent>();
        for (auto [entity, tc, mc] : view.each()) {
            if (mc.mesh_type != hz::MeshComponent::MeshType::Primitive) {
                continue;
            }

            // Check if grid sphere
            bool is_grid = false;
            if (auto* tag = m_scene->registry().try_get<hz::TagComponent>(entity)) {
                if (tag->tag == "GridSphere") {
                    is_grid = true;
                }
            }
            if (is_grid && !m_show_grid) {
                continue;
            }

            m_geometry_shader->set_mat4("u_Model", tc.get_transform());
            m_geometry_shader->set_bool("u_UseAlbedoMap", false);
            m_geometry_shader->set_bool("u_UseNormalMap", false);
            m_geometry_shader->set_bool("u_UseMetallicRoughnessMap", false);
            m_geometry_shader->set_bool("u_UseAOMap", false);
            m_geometry_shader->set_bool("u_UseEmissionMap", false);
            m_geometry_shader->set_vec3("u_AlbedoColor", mc.albedo_color);
            m_geometry_shader->set_float("u_Metallic", mc.metallic);
            m_geometry_shader->set_float("u_Roughness", mc.roughness);
            m_geometry_shader->set_float("u_MaterialID", 1.0f);
            m_geometry_shader->set_vec3("u_EmissionColor", glm::vec3(0.0f));
            m_geometry_shader->set_float("u_EmissionStrength", 0.0f);

            if (mc.primitive_name == "sphere" && m_sphere_mesh) {
                m_sphere_mesh->draw();
            } else if (mc.primitive_name == "cube" && m_cube_mesh) {
                m_cube_mesh->draw();
            }
        }
    }

    // Render treasure chest
    if (m_show_model && m_test_model && m_test_model->is_valid()) {
        auto view = m_scene->registry().view<hz::TransformComponent, hz::MeshComponent>();
        for (auto [entity, tc, mc] : view.each()) {
            if (mc.mesh_type == hz::MeshComponent::MeshType::Model && mc.model.index == 0) {
                glDisable(GL_CULL_FACE);
                if (m_albedo_tex)
                    m_albedo_tex->bind(0);
                if (m_normal_tex)
                    m_normal_tex->bind(1);
                if (m_arm_tex)
                    m_arm_tex->bind(2);

                m_geometry_shader->set_mat4("u_Model", tc.get_transform());
                m_geometry_shader->set_bool("u_UseAlbedoMap",
                                            m_albedo_tex && m_albedo_tex->is_valid());
                m_geometry_shader->set_bool("u_UseNormalMap",
                                            m_normal_tex && m_normal_tex->is_valid());
                m_geometry_shader->set_bool("u_UseMetallicRoughnessMap",
                                            m_arm_tex && m_arm_tex->is_valid());
                m_geometry_shader->set_bool("u_UseAOMap", false);
                m_geometry_shader->set_bool("u_UseEmissionMap", false);
                m_geometry_shader->set_vec3("u_AlbedoColor", glm::vec3(1.0f));
                m_geometry_shader->set_float("u_Metallic", 1.0f);
                m_geometry_shader->set_float("u_Roughness", 0.5f);
                m_geometry_shader->set_float("u_MaterialID", 1.0f);
                m_geometry_shader->set_vec3("u_EmissionColor", glm::vec3(0.0f));
                m_geometry_shader->set_float("u_EmissionStrength", 0.0f);

                m_test_model->draw();
                glEnable(GL_CULL_FACE);
            }
        }
    }

    // Render character
    if (m_character_model) {
        auto view = m_scene->registry().view<hz::TransformComponent, hz::MeshComponent>();
        for (auto entity : view) {
            auto& tc = view.get<hz::TransformComponent>(entity);
            auto& mc = view.get<hz::MeshComponent>(entity);

            if (mc.mesh_type != hz::MeshComponent::MeshType::Model || mc.model.index != 1) {
                continue;
            }

            m_geometry_shader->set_mat4("u_Model", tc.get_transform());

            bool has_anim = false;
            if (auto* ac = m_scene->registry().try_get<hz::AnimatorComponent>(entity)) {
                if (!ac->bone_transforms.empty()) {
                    has_anim = true;
                    m_geometry_shader->set_mat4_array("u_BoneMatrices", ac->bone_transforms.data(),
                                                      ac->bone_transforms.size());
                }
            }
            m_geometry_shader->set_bool("u_HasAnimation", has_anim);

            // Materials
            bool has_albedo = false, has_normal = false, has_mr = false;
            bool has_ao = false, has_emission = false;

            if (m_character_model->has_fbx_materials() &&
                !m_character_model->fbx_materials().empty()) {
                const auto& mat = m_character_model->fbx_materials()[0];
                if (mat.albedo_texture && mat.albedo_texture->is_valid()) {
                    mat.albedo_texture->bind(0);
                    has_albedo = true;
                }
                if (mat.normal_texture && mat.normal_texture->is_valid()) {
                    mat.normal_texture->bind(1);
                    has_normal = true;
                }
                if (mat.metallic_roughness_texture && mat.metallic_roughness_texture->is_valid()) {
                    mat.metallic_roughness_texture->bind(2);
                    has_mr = true;
                }
                if (mat.ao_texture && mat.ao_texture->is_valid()) {
                    mat.ao_texture->bind(3);
                    has_ao = true;
                }
                if (mat.emissive_texture && mat.emissive_texture->is_valid()) {
                    mat.emissive_texture->bind(4);
                    has_emission = true;
                }
                m_geometry_shader->set_vec3("u_AlbedoColor", mat.albedo_color);
                m_geometry_shader->set_float("u_Metallic", mat.metallic);
                m_geometry_shader->set_float("u_Roughness", mat.roughness);
            } else {
                m_geometry_shader->set_vec3("u_AlbedoColor", glm::vec3(0.8f, 0.7f, 0.6f));
                m_geometry_shader->set_float("u_Metallic", 0.0f);
                m_geometry_shader->set_float("u_Roughness", 0.8f);
            }

            m_geometry_shader->set_bool("u_UseAlbedoMap", has_albedo);
            m_geometry_shader->set_bool("u_UseNormalMap", has_normal);
            m_geometry_shader->set_bool("u_UseMetallicRoughnessMap", has_mr);
            m_geometry_shader->set_bool("u_UseAOMap", has_ao);
            m_geometry_shader->set_bool("u_UseEmissionMap", has_emission);

            glDisable(GL_CULL_FACE);
            m_character_model->draw();
            glEnable(GL_CULL_FACE);
            m_geometry_shader->set_bool("u_HasAnimation", false);
        }
    }

    m_renderer->end_geometry_pass();

    // === Lighting Pass ===
    glm::vec3 sun_color = glm::vec3(1.0f, 0.9f, 0.8f);
    m_renderer->execute_lighting_pass(camera, point_lights, spot_lights, sun_dir, sun_color,
                                      m_irradiance_map, m_prefilter_map, m_brdf_lut,
                                      m_environment_map);

    // === TAA ===
    m_renderer->execute_taa_pass();

    // === Final Output ===
    m_renderer->render_to_screen();

    // === Debug Skeleton ===
    if (m_show_skeleton && m_character_model && m_character_model->is_valid() &&
        m_character_model->has_skeleton()) {
        auto view = m_scene->registry()
                        .view<hz::TransformComponent, hz::MeshComponent, hz::AnimatorComponent>();
        for (auto [entity, tc, mc, ac] : view.each()) {
            if (mc.mesh_type == hz::MeshComponent::MeshType::Model && mc.model.index == 1) {
                m_debug_renderer->draw_skeleton(*m_character_model->skeleton(), ac.bone_transforms,
                                                tc.get_transform(), glm::vec3(0.0f, 1.0f, 0.0f),
                                                glm::vec3(1.0f, 1.0f, 0.0f));
            }
        }
        glm::mat4 debug_vp =
            camera.projection_matrix(GameConfig::ASPECT_RATIO) * camera.view_matrix();
        m_debug_renderer->render(debug_vp);
    }

    // === IK Visualization ===
    if (m_animation_system.is_ik_enabled()) {
        m_debug_renderer->draw_point(m_ik_target_position, 0.1f, glm::vec3(1.0f, 0.0f, 0.0f));
        m_debug_renderer->draw_axes(m_ik_target_position, 0.3f);
        glm::mat4 debug_vp =
            camera.projection_matrix(GameConfig::ASPECT_RATIO) * camera.view_matrix();
        m_debug_renderer->render(debug_vp);
    }

    // === UI ===
    m_imgui->begin_frame();
    ImGui::Begin("PBR Test");
    ImGui::Text("Profiling: %.2f ms", 1000.0f / ImGui::GetIO().Framerate);
    ImGui::Checkbox("Show sphere grid", &m_show_grid);
    ImGui::Checkbox("Show test model (treasure_chest)", &m_show_model);
    ImGui::Checkbox("Show skeleton debug", &m_show_skeleton);

    bool ik_enabled = m_animation_system.is_ik_enabled();
    if (ImGui::Checkbox("IK Demo", &ik_enabled)) {
        m_animation_system.set_ik_enabled(ik_enabled);
    }
    if (ik_enabled) {
        ImGui::DragFloat3("IK Target", &m_ik_target_position.x, 0.05f);
    }

    if (m_show_model) {
        auto view = m_scene->registry().view<hz::TransformComponent, hz::MeshComponent>();
        for (auto [entity, tc, mc] : view.each()) {
            if (mc.mesh_type == hz::MeshComponent::MeshType::Model && mc.model.index == 0) {
                ImGui::SeparatorText("Model transform");
                ImGui::DragFloat3("Position", &tc.position.x, 0.05f);
                ImGui::DragFloat3("Rotation", &tc.rotation.x, 1.0f);
                ImGui::DragFloat3("Scale", &tc.scale.x, 0.05f, 0.01f, 50.0f);
            }
            if (mc.mesh_type == hz::MeshComponent::MeshType::Model && mc.model.index == 1) {
                ImGui::SeparatorText("Character Transform");
                ImGui::DragFloat3("Char Pos", &tc.position.x, 0.05f);
                ImGui::DragFloat3("Char Rot", &tc.rotation.x, 1.0f);
                ImGui::DragFloat3("Char Scale", &tc.scale.x, 0.001f, 0.001f, 2.0f);
            }
        }
    }

    ImGui::Text("Controls:");
    ImGui::BulletText("WASD: Move");
    ImGui::BulletText("Space: Jump");
    ImGui::BulletText("Left Click: Shoot (Physics Impulse)");
    ImGui::BulletText("Tab: Toggle Mouse Cursor");
    ImGui::End();

    // Crosshair
    {
        auto* draw_list = ImGui::GetForegroundDrawList();
        ImVec2 center =
            ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f);
        draw_list->AddCircleFilled(center, 3.0f, IM_COL32(255, 255, 255, 200));
        draw_list->AddCircle(center, 4.0f, IM_COL32(0, 0, 0, 200));
    }

    m_imgui->end_frame();

    m_window->swap_buffers();
    glfwPollEvents();
}

void Application::shutdown() {
    m_imgui->shutdown();
    m_renderer->shutdown();
    m_physics->shutdown();
    m_audio->shutdown();
    hz::MemoryContext::shutdown();
    hz::Log::shutdown();
}

} // namespace game
