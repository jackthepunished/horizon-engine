/**
 * @file main.cpp
 * @brief Sample FPS game demonstrating the Horizon Engine
 *
 * This example creates a window with an FPS camera, renders a grid plane,
 * a textured cube, a physics-simulated falling cube, and allows WASD movement.
 */

#include "editor_ui.hpp"

#include <backends/imgui_impl_opengl3.h>
#include <engine/assets/asset_registry.hpp>
#include <engine/assets/cubemap.hpp>
#include <engine/assets/texture.hpp>
#include <engine/audio/audio_engine.hpp>
#include <engine/core/game_loop.hpp>
#include <engine/core/log.hpp>
#include <engine/core/memory.hpp>
#include <engine/ecs/world.hpp>
#include <engine/physics/physics_world.hpp>
#include <engine/platform/input.hpp>
#include <engine/platform/window.hpp>
#include <engine/renderer/camera.hpp>
#include <engine/renderer/mesh.hpp>
#include <engine/renderer/opengl/shader.hpp>
#include <engine/renderer/renderer.hpp>
#include <engine/scene/components.hpp>
#include <engine/scene/scene_serializer.hpp>
#include <engine/ui/debug_overlay.hpp>
#include <engine/ui/imgui_layer.hpp>
#include <imgui.h>

#define GLFW_INCLUDE_NONE
#include <fstream>
#include <sstream>

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

static std::string read_file(const std::string& path) {
    std::ifstream in(path, std::ios::in | std::ios::binary);
    if (!in) {
        HZ_ERROR("Could not open file: {}", path);
        return "";
    }
    std::stringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

// ============================================================================
// Shaders
// ============================================================================

static const char* GRID_VERTEX_SHADER = R"(
#version 410 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_texcoord;

uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_projection;

out vec3 v_world_pos;
out vec3 v_normal;
out vec2 v_texcoord;

void main() {
    vec4 world_pos = u_model * vec4(a_position, 1.0);
    v_world_pos = world_pos.xyz;
    v_normal = mat3(u_model) * a_normal;
    v_texcoord = a_texcoord;
    gl_Position = u_projection * u_view * world_pos;
}
)";

static const char* GRID_FRAGMENT_SHADER = R"(
#version 410 core

in vec3 v_world_pos;
in vec3 v_normal;
in vec2 v_texcoord;

out vec4 frag_color;

uniform vec3 u_camera_pos;

float grid(vec2 pos, float scale) {
    vec2 grid_pos = fract(pos * scale);
    float line_width = 0.02 * scale;
    float x_line = step(grid_pos.x, line_width) + step(1.0 - line_width, grid_pos.x);
    float z_line = step(grid_pos.y, line_width) + step(1.0 - line_width, grid_pos.y);
    return max(x_line, z_line);
}

void main() {
    vec3 base_color = vec3(0.15, 0.15, 0.18);

    float grid_small = grid(v_world_pos.xz, 1.0);
    float grid_large = grid(v_world_pos.xz, 0.1);

    vec3 color = base_color;
    color = mix(color, vec3(0.3, 0.3, 0.35), grid_small * 0.6);
    color = mix(color, vec3(0.5, 0.5, 0.6), grid_large * 0.9);

    float dist = length(v_world_pos.xz - u_camera_pos.xz);
    float fade = 1.0 - smoothstep(20.0, 80.0, dist);

    float fog_dist = length(v_world_pos - u_camera_pos);
    float fog = 1.0 - exp(-0.008 * fog_dist);
    vec3 fog_color = vec3(0.2, 0.3, 0.5);
    color = mix(color, fog_color, fog * 0.7);

    frag_color = vec4(color, fade);
}
)";

// ============================================================================
// Application
// ============================================================================

// ============================================================================
// Application
// ============================================================================

class Application {
public:
    void run() {
        hz::Log::init();
        HZ_LOG_INFO("Horizon Engine Sample Game starting...");

        hz::MemoryContext::init();

        // Create window
        hz::WindowConfig window_config;
        window_config.title = "Horizon Engine - Physics Demo";
        window_config.width = 1920;
        window_config.height = 1080;

        hz::Window window(window_config);

        // Setup input
        hz::InputManager input;
        input.attach(window);

        input.bind_key(hz::InputManager::ACTION_MOVE_FORWARD, GLFW_KEY_W);
        input.bind_key(hz::InputManager::ACTION_MOVE_BACKWARD, GLFW_KEY_S);
        input.bind_key(hz::InputManager::ACTION_MOVE_LEFT, GLFW_KEY_A);
        input.bind_key(hz::InputManager::ACTION_MOVE_RIGHT, GLFW_KEY_D);
        input.bind_key(hz::InputManager::ACTION_JUMP, GLFW_KEY_SPACE);
        input.bind_key(hz::InputManager::ACTION_CROUCH, GLFW_KEY_LEFT_CONTROL);
        input.bind_key(hz::InputManager::ACTION_SPRINT, GLFW_KEY_LEFT_SHIFT);
        input.bind_key(hz::InputManager::ACTION_MENU, GLFW_KEY_ESCAPE);

        // Create renderer
        hz::Renderer renderer(window);
        renderer.set_clear_color(0.02f, 0.02f, 0.05f); // Dark night sky

        // Create camera
        // Create camera
        // Calculate camera position to look at the grid (approx center is 0,1,0)
        // Constructor: Pos, Up, Yaw, Pitch
        hz::Camera camera(glm::vec3(0.0f, 8.0f, 12.0f), glm::vec3(0.0f, 1.0f, 0.0f), -90.0f,
                          -30.0f);
        camera.movement_speed = 8.0f;

        // Create shaders
        // Create shaders
        hz::gl::Shader grid_shader(GRID_VERTEX_SHADER, GRID_FRAGMENT_SHADER);

        std::string phong_vert = read_file("assets/shaders/phong.vert");
        std::string phong_frag = read_file("assets/shaders/phong.frag");
        hz::gl::Shader phong_shader(phong_vert, phong_frag);
        phong_shader.bind();
        phong_shader.set_float("u_uv_scale", 1.0f);

        std::string depth_vert = read_file("assets/shaders/depth.vert");
        std::string depth_frag = read_file("assets/shaders/depth.frag");
        hz::gl::Shader depth_shader(depth_vert, depth_frag);

        std::string quad_vert = read_file("assets/shaders/quad.vert");
        std::string hdr_frag = read_file("assets/shaders/hdr.frag");
        hz::gl::Shader hdr_shader(quad_vert, hdr_frag);
        hdr_shader.bind();
        hdr_shader.set_int("u_hdr_buffer", 0);
        hdr_shader.set_int("u_bloom_blur", 1);
        hdr_shader.set_float("u_exposure", 1.0f);
        hdr_shader.set_float("u_bloom_intensity", 0.5f);
        hdr_shader.set_bool("u_bloom_enabled", true);

        // Bloom shaders
        std::string bloom_extract_frag = read_file("assets/shaders/bloom_extract.frag");
        hz::gl::Shader bloom_extract_shader(quad_vert, bloom_extract_frag);

        std::string blur_frag = read_file("assets/shaders/blur.frag");
        hz::gl::Shader blur_shader(quad_vert, blur_frag);

        // Skybox shader (HDRI)
        std::string skybox_vert = read_file("assets/shaders/skybox_hdr.vert");
        std::string skybox_frag = read_file("assets/shaders/skybox_hdr.frag");
        hz::gl::Shader skybox_shader(skybox_vert, skybox_frag);
        skybox_shader.bind();
        skybox_shader.set_int("u_skybox_map", 0);

        // PBR shader
        std::string pbr_vert = read_file("assets/shaders/pbr.vert");
        std::string pbr_frag = read_file("assets/shaders/pbr.frag");
        hz::gl::Shader pbr_shader(pbr_vert, pbr_frag);

        // Geometry shader (SSAO Prepass)
        std::string geom_vert = read_file("assets/shaders/geometry.vert");
        std::string geom_frag = read_file("assets/shaders/geometry.frag");
        hz::gl::Shader geometry_shader(geom_vert, geom_frag);

        // SSAO Shader
        std::string ssao_vert = read_file("assets/shaders/ssao.vert");
        std::string ssao_frag = read_file("assets/shaders/ssao.frag");
        hz::gl::Shader ssao_shader(ssao_vert, ssao_frag);

        // SSAO Blur Shader
        std::string blur_frag_src = read_file("assets/shaders/ssao_blur.frag");
        hz::gl::Shader ssao_blur_shader(ssao_vert, blur_frag_src);

        // Outline shader for selection highlight
        std::string outline_vert = read_file("assets/shaders/outline.vert");
        std::string outline_frag = read_file("assets/shaders/outline.frag");
        hz::gl::Shader outline_shader(outline_vert, outline_frag);

        pbr_shader.bind();
        pbr_shader.set_int("u_albedo_map", 0);
        pbr_shader.set_int("u_normal_map", 1);
        pbr_shader.set_int("u_metallic_map", 2);
        pbr_shader.set_int("u_roughness_map", 3);
        pbr_shader.set_int("u_ao_map", 4);
        pbr_shader.set_int("u_shadow_map", 5);
        pbr_shader.set_int("u_ssao_map", 6);
        pbr_shader.set_int("u_ao_map", 4);
        pbr_shader.set_int("u_shadow_map", 5);
        pbr_shader.set_int("u_ssao_map", 6);
        pbr_shader.set_int("u_ao_map", 4);
        pbr_shader.set_int("u_shadow_map", 5);

        // Create meshes
        hz::Mesh ground_cube = hz::Mesh::create_cube(1.0f);
        hz::Mesh cube = hz::Mesh::create_cube(2.0f);
        hz::Mesh physics_cube_mesh = hz::Mesh::create_cube(1.0f);
        hz::Mesh skybox_cube = hz::Mesh::create_sphere(50.0f); // Large sphere for Skybox

        // Load HDRI texture
        hz::AssetRegistry assets;
        hz::TextureHandle skybox_tex_handle =
            assets.load_texture("assets/textures/skybox/hdri_night_12k.jpg");

        hz::TextureHandle tex_handle = assets.load_texture("assets/textures/test.jpg");
        hz::TextureHandle normal_map_handle =
            assets.load_texture("assets/textures/wood_normal.png");
        hz::TextureHandle metal_albedo_handle =
            assets.load_texture("assets/textures/metal_albedo.png");

        // Custom PBR Textures
        hz::TextureHandle custom_albedo =
            assets.load_texture("assets/textures/custom_pbr/albedo.png");
        hz::TextureHandle custom_normal =
            assets.load_texture("assets/textures/custom_pbr/normal.png");
        hz::TextureHandle custom_metallic =
            assets.load_texture("assets/textures/custom_pbr/metallic.png");
        hz::TextureHandle custom_roughness =
            assets.load_texture("assets/textures/custom_pbr/roughness.png");
        hz::TextureHandle custom_ao = assets.load_texture("assets/textures/custom_pbr/ao.png");

        // Initialize Audio
        hz::AudioSystem audio;
        audio.init();
        // hz::SoundHandle impact_sound = assets.load_sound("assets/audio/impact.wav", audio);

        // ==========================================
        // Initialize Physics
        // ==========================================
        hz::PhysicsWorld physics;
        physics.init();

        // Create static ground (invisible - just for collision)
        physics.create_static_box(glm::vec3(0.0f, -0.5f, 0.0f), glm::vec3(50.0f, 0.5f, 50.0f));

        // Create dynamic falling cube (Removed for Showcase)
        // const glm::vec3 CUBE_SPAWN_POS(3.0f, 10.0f, -5.0f);
        // hz::PhysicsBodyID falling_cube =
        //     physics.create_dynamic_box(CUBE_SPAWN_POS, glm::vec3(0.5f), 1.0f);

        // Reset timer
        // float reset_timer = 0.0f;
        bool waiting_to_reset = false;

        HZ_LOG_INFO("Scene created with physics!");

        // ==========================================
        // Setup Lighting
        // ==========================================
        hz::SceneLighting lighting;
        lighting.sun.direction = glm::normalize(glm::vec3(-0.2f, -1.0f, -0.3f));
        lighting.sun.color = glm::vec3(1.0f, 0.95f, 0.8f);
        lighting.sun.intensity = 1.0f;
        lighting.ambient_light = glm::vec3(0.15f);

        lighting.point_lights.push_back({}); // Light 0
        lighting.point_lights[0].color = glm::vec3(1.0f, 0.2f, 0.1f);
        lighting.point_lights[0].intensity = 2.0f;
        lighting.point_lights[0].range = 15.0f;

        hz::ShadowSettings shadow_settings;
        shadow_settings.resolution = 2048; // High res
        shadow_settings.enabled = true;
        renderer.set_shadow_settings(shadow_settings);

        // ==========================================
        // Initialize ImGui
        // ==========================================        // Initialize ImGui
        hz::ImGuiLayer imgui;
        imgui.init(window);

        hz::DebugOverlay debug_overlay;
        hz::ActionId toggle_debug_action = input.register_action("toggle_debug");
        input.bind_key(toggle_debug_action, GLFW_KEY_F3);

        // In-Game Editor
        game::EditorUI editor;

        // Custom sink for editor console - Simplified manual logging for now
        // To properly hook spdlog, we'd need to include spdlog/sinks/callback_sink.h which might
        // not be available in the bundled version or requires extra setup. For this demo, we can
        // manually log specific events to the editor.
        editor.add_log("Welcome to Horizon Engine Editor!");
        editor.add_log("Press Tab to toggle cursor.");

        game::SceneSettings settings;

        // ECS World
        hz::World world;

        // Entities
        // Entities
        auto e_floor = world.create_entity();
        world.add_component<hz::TagComponent>(e_floor, "Floor");
        auto& t_floor = world.add_component<hz::TransformComponent>(e_floor);
        t_floor.position = glm::vec3(0.0f, -0.5f, 0.0f);
        t_floor.scale = glm::vec3(100.0f, 1.0f, 100.0f); // Scale 100 for floor
        auto& m_floor = world.add_component<hz::MeshComponent>(e_floor);
        m_floor.mesh_path = "plane";
        m_floor.albedo_path = "brick"; // Keep brick floor

        // Create 5x5 Grid of Cubes
        // Rows (Z): Roughness (0.0 to 1.0)
        // Cols (X): Metallic (0.0 to 1.0)
        int rows = 5;
        int cols = 5;
        float spacing = 2.5f;

        // Track dynamic cubes: Entity + PhysicsID + SpawnPos
        struct FallingCube {
            hz::Entity entity;
            hz::PhysicsBodyID body_id;
            glm::vec3 spawn_pos;
        };
        std::vector<FallingCube> falling_cubes;

        for (int r = 0; r < rows; ++r) {
            float roughness = glm::clamp(static_cast<float>(r) / (rows - 1), 0.05f, 1.0f);
            for (int c = 0; c < cols; ++c) {
                float metallic = static_cast<float>(c) / (cols - 1);

                auto entity = world.create_entity();
                world.add_component<hz::TagComponent>(entity, "PBR Cube " + std::to_string(r) +
                                                                  "-" + std::to_string(c));

                bool is_falling = (r + c) % 2 == 0; // Checkerboard pattern
                glm::vec3 pos;

                if (is_falling) {
                    pos = glm::vec3((c - (cols / 2.0f)) * spacing,
                                    8.0f + (rand() % 5), // Randomize drop height slightly
                                    (r - (rows / 2.0f)) * spacing);
                } else {
                    pos = glm::vec3((c - (cols / 2.0f)) * spacing, 1.0f,
                                    (r - (rows / 2.0f)) * spacing);
                }

                auto& tc = world.add_component<hz::TransformComponent>(entity);
                tc.position = pos;

                // Add Physics Body if falling
                if (is_falling) {
                    hz::PhysicsBodyID body = physics.create_dynamic_box(pos, glm::vec3(0.5f), 1.0f);
                    falling_cubes.push_back({entity, body, pos});
                }

                auto& mc = world.add_component<hz::MeshComponent>(entity);
                mc.mesh_path = "cube";
                mc.albedo_color = glm::vec3(1.0f, 0.0f, 0.0f); // Red base color
                mc.metallic = metallic;
                mc.roughness = roughness;
                mc.albedo_path = ""; // Use solid color
            }
        }

        // ==========================================
        // Load Demon Weapon (GLTF)
        // ==========================================
        // ==========================================
        // Load Demon Weapon (GLTF)
        // ==========================================
        // ==========================================
        // Load Demon Weapon (GLTF)
        // ==========================================
        hz::Entity weapon_entity; // Default constructor (invalid)
        hz::Model weapon_model = hz::Model::load_from_gltf("assets/models/demon_weapon/scene.gltf");

        if (weapon_model.is_valid()) {
            weapon_entity = world.create_entity();
            world.add_component<hz::TagComponent>(weapon_entity, "Demon Weapon");
            auto& tc = world.add_component<hz::TransformComponent>(weapon_entity);

            // "Left of the boxes" -> Boxes are at X ~ -5 to 5. Let's put it at -10.
            // "Mid-air" -> Y = 5.0
            tc.position = glm::vec3(-12.0f, 5.0f, 0.0f);
            tc.scale = glm::vec3(5.0f);
            // tc.rotation = glm::vec3(90.0f, 0.0f, 0.0f);

            auto& mc = world.add_component<hz::MeshComponent>(weapon_entity);
            mc.mesh_path = "assets/models/demon_weapon/scene.gltf";
            mc.albedo_color = glm::vec3(1.0f);
            mc.metallic = 0.5f;
            mc.roughness = 0.5f;
        }

        // FPS tracking
        float fps = 0.0f;
        float frame_time = 0.0f;
        float fps_timer = 0.0f;
        int frame_count = 0;

        window.set_cursor_captured(true);

        // Game loop
        hz::GameLoop loop;

        loop.set_should_quit_callback([&window, &input]() {
            return window.should_close() ||
                   input.is_action_just_pressed(hz::InputManager::ACTION_MENU);
        });

        loop.set_input_callback([&window, &input]() {
            window.poll_events();
            input.update();

            // Toggle cursor capture with Tab (for editor interaction)
            static bool tab_held = false;
            if (glfwGetKey(window.native_handle(), GLFW_KEY_TAB) == GLFW_PRESS) {
                if (!tab_held) {
                    bool captured = window.is_cursor_captured();
                    window.set_cursor_captured(!captured);
                    tab_held = true;
                }
            } else {
                tab_held = false;
            }
        });

        loop.set_update_callback([&](hz::f64 dt) {
            float dt_f = static_cast<float>(dt);

            // Rotate Weapon
            if (world.is_alive(weapon_entity)) {
                if (auto* tc = world.get_component<hz::TransformComponent>(weapon_entity)) {
                    // Rotate around Y axis
                    tc->rotation.y += 45.0f * dt_f;

                    // Bob up and down slightly
                    tc->position.y =
                        5.0f + std::sin(static_cast<float>(glfwGetTime()) * 2.0f) * 0.5f;
                }
            }

            // Update FPS
            fps_timer += dt_f;
            frame_count++;
            if (fps_timer >= 1.0f) {
                fps = static_cast<float>(frame_count) / fps_timer;
                frame_time = 1000.0f / fps;
                fps_timer = 0.0f;
                frame_count = 0;
            }

            // Toggle debug overlay
            if (input.is_action_just_pressed(toggle_debug_action)) {
                debug_overlay.toggle();
            }

            // SERIALIZATION TEST
            // F5 to Save
            if (glfwGetKey(window.native_handle(), GLFW_KEY_F5) == GLFW_PRESS) {
                static bool f5_pressed = false;
                if (!f5_pressed) {
                    hz::SceneSerializer serializer(world);
                    serializer.serialize("scene.json");
                    HZ_LOG_INFO("Saved scene to scene.json");
                    f5_pressed = true;
                }
            } else {
                static bool f5_pressed = false;
                f5_pressed = false;
            }

            // F6 to Load
            if (glfwGetKey(window.native_handle(), GLFW_KEY_F6) == GLFW_PRESS) {
                static bool f6_pressed = false;
                if (!f6_pressed) {
                    hz::SceneSerializer serializer(world);
                    if (serializer.deserialize("scene.json")) {
                        HZ_LOG_INFO("Loaded scene from scene.json");
                    }
                    f6_pressed = true;
                }
            } else {
                static bool f6_pressed = false;
                f6_pressed = false;
            }

            // Update physics
            physics.update(dt_f);

            // Sync Falling Cubes
            for (const auto& fc : falling_cubes) {
                // Get Pos
                glm::vec3 pos = physics.get_body_position(fc.body_id);
                glm::quat rot = physics.get_body_rotation(fc.body_id);

                // Sync to ECS
                if (auto* tc = world.get_component<hz::TransformComponent>(fc.entity)) {
                    tc->position = pos;
                    (void)rot; // Silence unused warning
                }

                // Reset if too low
                if (pos.y < -5.0f) {
                    // Reset Physics Body
                    physics.set_body_position(fc.body_id, fc.spawn_pos);
                    physics.set_body_velocity(fc.body_id, glm::vec3(0.0f));
                    // physics.set_body_angular_velocity(fc.body_id, glm::vec3(0.0f)); // Not
                    // implemented yet, optional
                }
            }

            // Reset timer (Unused in showcase)
            if (waiting_to_reset) {
                waiting_to_reset = false;
            }

            // Animate Point Light
            lighting.point_lights[0].position =
                glm::vec3(3.0f * cos(fps_timer), 2.0f, 3.0f * sin(fps_timer));

            // Camera movement
            glm::vec3 move_dir{0.0f};

            if (input.is_action_active(hz::InputManager::ACTION_MOVE_FORWARD)) {
                move_dir.z = 1.0f;
            }
            if (input.is_action_active(hz::InputManager::ACTION_MOVE_BACKWARD)) {
                move_dir.z = -1.0f;
            }
            if (input.is_action_active(hz::InputManager::ACTION_MOVE_LEFT)) {
                move_dir.x = -1.0f;
            }
            if (input.is_action_active(hz::InputManager::ACTION_MOVE_RIGHT)) {
                move_dir.x = 1.0f;
            }
            if (input.is_action_active(hz::InputManager::ACTION_JUMP)) {
                move_dir.y = 1.0f;
            }
            if (input.is_action_active(hz::InputManager::ACTION_CROUCH)) {
                move_dir.y = -1.0f;
            }

            if (input.is_action_active(hz::InputManager::ACTION_SPRINT)) {
                camera.movement_speed = 16.0f;
            } else {
                camera.movement_speed = 8.0f;
            }

            camera.process_movement(move_dir, dt_f);

            // Only process mouse for camera when cursor is captured
            if (window.is_cursor_captured()) {
                const auto& mouse = input.mouse();
                camera.process_mouse(static_cast<float>(mouse.delta_x),
                                     static_cast<float>(-mouse.delta_y));
            }
        });

        loop.set_render_callback([&](hz::f64 alpha) {
            HZ_UNUSED(alpha);

            renderer.begin_frame();

            auto [width, height] = renderer.framebuffer_size();
            float aspect = static_cast<float>(width) / static_cast<float>(height);

            glm::mat4 view = camera.view_matrix();
            glm::mat4 projection = camera.projection_matrix(aspect);

            // ========================================
            // 0. Geometry Pass (Render Normals/Depth)
            // ========================================
            renderer.begin_geometry_pass();
            geometry_shader.bind();
            geometry_shader.set_mat4("u_view", view);
            geometry_shader.set_mat4("u_projection", projection);

            // DRAW SCENE (OPAQUE ONLY)
            // Ideally we wrap this in a lambda or function
            // DRAW SCENE (OPAQUE ONLY) - ECS
            world.each_entity([&](hz::Entity entity) {
                // HZ_LOG_INFO("Ren. ent: {}", entity.index); // Commented out to reduce spam
                auto* tc = world.get_component<hz::TransformComponent>(entity);
                auto* mc = world.get_component<hz::MeshComponent>(entity);

                if (tc && mc) {
                    geometry_shader.set_mat4("u_model", tc->get_transform());
                    if (mc->mesh_path == "cube") {
                        cube.draw();
                    } else if (mc->mesh_path == "plane") {
                        ground_cube.draw();
                    }
                }
            });
            renderer.end_geometry_pass();

            // ========================================
            // 1. SSAO Pass
            // ========================================
            renderer.render_ssao(ssao_shader, projection);

            // ========================================
            // 2. SSAO Blur Pass
            // ========================================
            renderer.render_ssao_blur(ssao_blur_shader);

            // ========================================
            // 1. Render opaque objects
            // ========================================

            // Rotating textured cube
            if (auto* tex = assets.get_texture(tex_handle)) {
                tex->bind(0);
            }

            glm::mat4 cube_model = glm::mat4(1.0f);
            cube_model = glm::translate(cube_model, glm::vec3(0.0f, 2.0f, -5.0f));

            static float rotation = 0.0f;
            rotation += 0.5f;
            cube_model =
                glm::rotate(cube_model, glm::radians(rotation), glm::vec3(0.0f, 1.0f, 0.0f));

            // Static cube (for better normal map viewing)
            glm::mat4 static_cube_model = glm::mat4(1.0f);
            static_cube_model = glm::translate(static_cube_model, glm::vec3(-4.0f, 1.5f, -3.0f));
            static_cube_model = glm::scale(static_cube_model, glm::vec3(1.5f)); // Slightly larger

            // PBR metallic cube
            glm::mat4 pbr_cube_model = glm::mat4(1.0f);
            pbr_cube_model = glm::translate(pbr_cube_model, glm::vec3(4.0f, 1.5f, -3.0f));
            pbr_cube_model = glm::scale(pbr_cube_model, glm::vec3(1.5f));

            // Physics falling cube (Removed for Showcase)
            // glm::vec3 phys_pos = physics.get_body_position(falling_cube);
            // glm::quat phys_rot = physics.get_body_rotation(falling_cube);
            // glm::mat4 phys_model = glm::mat4(1.0f);
            // phys_model = glm::translate(phys_model, phys_pos);
            // phys_model = phys_model * glm::mat4_cast(phys_rot);

            // ========================================
            // 1. Shadow Pass
            // ========================================
            renderer.begin_shadow_pass();
            depth_shader.bind();
            glm::mat4 light_space_matrix = renderer.get_light_space_matrix();
            depth_shader.set_mat4("u_light_space_matrix", light_space_matrix);

            // Draw Objects that cast shadows (ECS)
            world.each_entity([&](hz::Entity entity) {
                auto* tc = world.get_component<hz::TransformComponent>(entity);
                auto* mc = world.get_component<hz::MeshComponent>(entity);

                if (tc && mc) {
                    depth_shader.set_mat4("u_model", tc->get_transform());
                    if (mc->mesh_path == "cube") {
                        cube.draw();
                    } else if (mc->mesh_path == "plane") {
                        // plane.draw(); // Floor usually doesn't cast shadows on itself in simple
                        // directional setups, but good for self-shadowing
                        ground_cube.draw();
                    }
                }
            });
            renderer.end_shadow_pass();

            // ========================================
            // 2. Scene Lighting Pass (Render to HDR FBO)
            // ========================================
            renderer.begin_scene_pass();

            // Submit and Apply Lighting
            renderer.submit_lighting(lighting);
            renderer.bind_shadow_map(
                5); // Bind shadow map to unit 5 to avoid collision with material textures

            // Bind SSAO texture manually (since we get ID)
            glActiveTexture(GL_TEXTURE0 + 6);
            glBindTexture(GL_TEXTURE_2D, renderer.get_ssao_blur_texture_id());

            pbr_shader.bind();
            renderer.apply_lighting(pbr_shader); // Apply light uniforms
            pbr_shader.set_mat4("u_view_projection", projection * view);
            pbr_shader.set_mat4("u_light_space_matrix", light_space_matrix);
            pbr_shader.set_vec3("u_view_pos", camera.position());

            // Set Texture Slots (Must match what we bind below)
            pbr_shader.set_int("u_albedo_map", 0);
            pbr_shader.set_int("u_normal_map", 1);
            pbr_shader.set_int("u_metallic_map", 2);
            pbr_shader.set_int("u_roughness_map", 3);
            pbr_shader.set_int("u_ao_map", 4);
            pbr_shader.set_int("u_shadow_map", 5); // Shadow map at 5

            pbr_shader.set_bool("u_use_normal_map", true);
            pbr_shader.set_bool("u_use_metallic_map", false);
            pbr_shader.set_bool("u_use_roughness_map", false);
            pbr_shader.set_bool("u_use_ao_map", false);
            // pbr_shader.set_vec3("u_specular_color", glm::vec3(0.5f)); // PBR doesn't use specular
            // color like Phong pbr_shader.set_float("u_shininess", 32.0f);

            // Render ECS Scene
            world.each_entity([&](hz::Entity entity) {
                auto* tc = world.get_component<hz::TransformComponent>(entity);
                auto* mc = world.get_component<hz::MeshComponent>(entity);

                if (tc && mc) {
                    // Set Model Matrix
                    pbr_shader.set_mat4("u_model", tc->get_transform());

                    // Set PBR Material Properties
                    pbr_shader.set_vec3("u_albedo", mc->albedo_color);
                    pbr_shader.set_float("u_metallic", mc->metallic);
                    pbr_shader.set_float("u_roughness", mc->roughness);
                    pbr_shader.set_float("u_ao", 1.0f);

                    // Texture Binding (Hardcoded mapping for demo assets)
                    bool use_textures = false;
                    bool use_normal_map = false;

                    if (mc->albedo_path == "wood") {
                        if (auto* t = assets.get_texture(tex_handle))
                            t->bind(0);
                        if (auto* t = assets.get_texture(normal_map_handle))
                            t->bind(1);
                        // Wood in this demo is just albedo+normal, no PBR maps loaded for it
                        // specifically So we use defaults or maybe reuse custom? For now disable
                        // other maps for wood to be safe
                        pbr_shader.set_bool("u_use_metallic_map", false);
                        pbr_shader.set_bool("u_use_roughness_map", false);
                        pbr_shader.set_bool("u_use_ao_map", false);

                        use_textures = true;
                        use_normal_map = true;
                    } else if (mc->albedo_path == "brick") {
                        // "Brick" in our Setup seems to be the Custom PBR set
                        if (auto* t = assets.get_texture(custom_albedo))
                            t->bind(0);
                        if (auto* t = assets.get_texture(custom_normal))
                            t->bind(1);
                        if (auto* t = assets.get_texture(custom_metallic))
                            t->bind(2);
                        if (auto* t = assets.get_texture(custom_roughness))
                            t->bind(3);
                        if (auto* t = assets.get_texture(custom_ao))
                            t->bind(4);

                        use_textures = true;
                        use_normal_map = true;
                        pbr_shader.set_bool("u_use_metallic_map", true);
                        pbr_shader.set_bool("u_use_roughness_map", true);
                        pbr_shader.set_bool("u_use_ao_map", true);

                        // Special tiling for floor
                        if (mc->mesh_path == "plane") {
                            pbr_shader.set_float("u_uv_scale", 100.0f);
                        }
                    } else if (mc->albedo_path == "metal") {
                        if (auto* t = assets.get_texture(metal_albedo_handle))
                            t->bind(0);
                        use_textures = true;
                        pbr_shader.set_bool("u_use_metallic_map", false);
                        pbr_shader.set_bool("u_use_roughness_map", false);
                        pbr_shader.set_bool("u_use_normal_map", false);
                    }

                    pbr_shader.set_bool("u_use_textures", use_textures);
                    pbr_shader.set_bool("u_use_normal_map", use_normal_map);

                    // DRAW
                    if (mc->mesh_path == "cube") {
                        cube.draw();
                    } else if (mc->mesh_path == "plane") {
                        ground_cube.draw();
                    } else if (mc->mesh_path == "assets/models/demon_weapon/scene.gltf") {
                        if (weapon_model.is_valid()) {
                            weapon_model.draw();
                        }
                    }

                    // Reset Tiling
                    pbr_shader.set_float("u_uv_scale", 1.0f);
                }
            });

            // ========================================
            // Render Skybox (last, with depth = 1.0)
            // ========================================
            glDepthFunc(GL_LEQUAL);  // Skybox passes where depth == 1.0
            glDisable(GL_CULL_FACE); // We're inside the sphere, don't cull
            skybox_shader.bind();
            skybox_shader.set_mat4("u_view", view);
            skybox_shader.set_mat4("u_projection", projection);

            if (auto* t = assets.get_texture(skybox_tex_handle)) {
                t->bind(0);
                skybox_shader.set_int("u_skybox_map", 0);
            }

            skybox_cube.draw();
            glEnable(GL_CULL_FACE); // Restore culling
            glDepthFunc(GL_LESS);   // Restore default

            // ========================================
            // Selection Outline Pass
            // ========================================
            if (editor.has_selection()) {
                hz::Entity selected = editor.selected_entity();
                auto* tc = world.get_component<hz::TransformComponent>(selected);
                auto* mc = world.get_component<hz::MeshComponent>(selected);

                if (tc && mc) {
                    // Render outline (back faces, slightly larger)
                    glEnable(GL_CULL_FACE);
                    glCullFace(GL_FRONT); // Render back faces only
                    glDepthFunc(GL_LEQUAL);

                    outline_shader.bind();
                    outline_shader.set_mat4("u_model", tc->get_transform());
                    outline_shader.set_mat4("u_view_projection", projection * view);
                    outline_shader.set_float("u_outline_thickness", 0.05f);
                    outline_shader.set_vec3("u_outline_color",
                                            glm::vec3(1.0f, 0.6f, 0.0f)); // Orange

                    if (mc->mesh_path == "cube") {
                        cube.draw();
                    } else if (mc->mesh_path == "plane") {
                        ground_cube.draw();
                    }

                    glCullFace(GL_BACK); // Restore
                    glDepthFunc(GL_LESS);
                }
            }

            renderer.end_scene_pass(); // Writes to HDR FBO

            // ========================================
            // 3. Bloom Pass (Extract + Blur)
            // ========================================
            renderer.render_bloom(bloom_extract_shader, blur_shader, 0.8f, 5);

            // ========================================
            // 4. Post-Process Pass (HDR + Bloom -> Screen)
            // ========================================
            hdr_shader.bind();
            glActiveTexture(GL_TEXTURE0 + 1);
            glBindTexture(GL_TEXTURE_2D, renderer.get_bloom_texture_id());
            renderer.render_post_process(hdr_shader);

            // ========================================
            // UI Overlay (ImGui)
            // ========================================
            // Update shaders with settings
            pbr_shader.bind();
            pbr_shader.set_vec3("u_sun_direction", settings.sun_direction);
            pbr_shader.set_vec3("u_sun_color", settings.sun_color);
            pbr_shader.set_float("u_sun_intensity", settings.sun_intensity);

            hdr_shader.bind();
            hdr_shader.set_float("u_exposure", settings.exposure);

            // ... (rest of rendering)

            // ========================================
            // UI Overlay (ImGui)
            // ========================================
            imgui.begin_frame();
            debug_overlay.draw(fps, frame_time, 2);
            editor.draw(world, settings, fps, 0); // TODO: Pass real entity count
            imgui.end_frame();

            // ========================================
            // Render transparent objects (e.g. Grid)
            // ========================================

            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDepthMask(GL_FALSE);

            glm::mat4 grid_model = glm::mat4(1.0f); // Renamed from ground_model to avoid conflict
            grid_shader.bind();
            grid_shader.set_mat4("u_model", grid_model);
            grid_shader.set_mat4("u_view", view);
            grid_shader.set_mat4("u_projection", projection);
            grid_shader.set_vec3("u_camera_pos", camera.position());
            ground_cube.draw();

            glDepthMask(GL_TRUE);
            glDisable(GL_BLEND);

            renderer.end_frame();
        });

        HZ_LOG_INFO("Starting game loop...");
        loop.run();

        imgui.shutdown();
        physics.shutdown();
        audio.shutdown();
        assets.clear();
        hz::MemoryContext::shutdown();
        hz::Log::shutdown();
    }
};

// ============================================================================
// Entry Point
// ============================================================================

int main() {
    try {
        Application app;
        app.run();
        return 0;
    } catch (const std::exception& e) {
        HZ_FATAL("Fatal error: {}", e.what());
        return 1;
    }
}
