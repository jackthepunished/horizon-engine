/**
 * @file main.cpp
 * @brief Sample FPS game demonstrating the Horizon Engine
 *
 * This example creates a window with an FPS camera, renders a grid plane,
 * a textured cube, a physics-simulated falling cube, and allows WASD movement.
 */

#include <engine/assets/asset_registry.hpp>
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
#include <engine/ui/debug_overlay.hpp>
#include <engine/ui/imgui_layer.hpp>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

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

// Textured/colored object shader
static const char* TEXTURED_VERTEX_SHADER = R"(
#version 410 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_texcoord;

uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_projection;

out vec3 v_normal;
out vec2 v_texcoord;

void main() {
    v_normal = mat3(u_model) * a_normal;
    v_texcoord = a_texcoord;
    gl_Position = u_projection * u_view * u_model * vec4(a_position, 1.0);
}
)";

static const char* TEXTURED_FRAGMENT_SHADER = R"(
#version 410 core

in vec3 v_normal;
in vec2 v_texcoord;

out vec4 frag_color;

uniform sampler2D u_texture;
uniform int u_use_texture;
uniform vec3 u_color;

void main() {
    vec3 light_dir = normalize(vec3(1.0, 1.0, 0.5));
    float diff = max(dot(normalize(v_normal), light_dir), 0.3);
    
    vec3 color;
    if (u_use_texture == 1) {
        color = texture(u_texture, v_texcoord).rgb;
    } else {
        color = u_color;
    }
    frag_color = vec4(color * diff, 1.0);
}
)";

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
        window_config.width = 1280;
        window_config.height = 720;

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
        renderer.set_clear_color(0.2f, 0.3f, 0.5f);

        // Create camera
        hz::Camera camera(glm::vec3(0.0f, 3.0f, 10.0f));
        camera.movement_speed = 8.0f;

        // Create shaders
        hz::gl::Shader grid_shader(GRID_VERTEX_SHADER, GRID_FRAGMENT_SHADER);
        hz::gl::Shader textured_shader(TEXTURED_VERTEX_SHADER, TEXTURED_FRAGMENT_SHADER);

        // Create meshes
        hz::Mesh ground_plane = hz::Mesh::create_plane(100.0f, 100);
        hz::Mesh cube = hz::Mesh::create_cube(2.0f);
        hz::Mesh physics_cube_mesh = hz::Mesh::create_cube(1.0f);

        // Load texture
        hz::AssetRegistry assets;
        hz::TextureHandle tex_handle = assets.load_texture("assets/textures/test.jpg");

        // Initialize Audio
        hz::AudioSystem audio;
        audio.init();
        hz::SoundHandle impact_sound = assets.load_sound("assets/audio/impact.wav", audio);

        // ==========================================
        // Initialize Physics
        // ==========================================
        hz::PhysicsWorld physics;
        physics.init();

        // Create static ground (invisible - just for collision)
        physics.create_static_box(glm::vec3(0.0f, -0.5f, 0.0f), glm::vec3(50.0f, 0.5f, 50.0f));

        // Create dynamic falling cube
        const glm::vec3 CUBE_SPAWN_POS(3.0f, 10.0f, -5.0f);
        hz::PhysicsBodyID falling_cube =
            physics.create_dynamic_box(CUBE_SPAWN_POS, glm::vec3(0.5f), 1.0f);

        // Reset timer
        float reset_timer = 0.0f;
        bool waiting_to_reset = false;

        HZ_LOG_INFO("Scene created with physics!");

        // ==========================================
        // Initialize ImGui
        // ==========================================
        hz::ImGuiLayer imgui;
        imgui.init(window);

        hz::DebugOverlay debug_overlay;
        hz::ActionId toggle_debug_action = input.register_action("toggle_debug");
        input.bind_key(toggle_debug_action, GLFW_KEY_F3);

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

            // Toggle mouse capture when menu is open or debug is active
            if (input.is_action_just_pressed(hz::InputManager::ACTION_MENU)) {
                window.set_cursor_captured(false);
            }
        });

        loop.set_update_callback([&](hz::f64 dt) {
            float dt_f = static_cast<float>(dt);

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

            // Update physics
            physics.update(dt_f);

            // Check if cube needs reset
            glm::vec3 cube_pos = physics.get_body_position(falling_cube);

            if (!waiting_to_reset && cube_pos.y < 1.5f) {
                // Cube landed, start reset timer
                waiting_to_reset = true;
                reset_timer = 0.0f;

                // Play impact sound (once)
                if (cube_pos.y < 0.6f) { // Only play if it actually hit the ground recently
                    audio.play(impact_sound);
                }
            }

            if (waiting_to_reset) {
                reset_timer += dt_f;
                if (reset_timer >= 1.0f) {
                    // Reset cube to spawn position
                    physics.remove_body(falling_cube);
                    falling_cube =
                        physics.create_dynamic_box(CUBE_SPAWN_POS, glm::vec3(0.5f), 1.0f);
                    waiting_to_reset = false;
                    reset_timer = 0.0f;
                }
            }

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

            const auto& mouse = input.mouse();
            camera.process_mouse(static_cast<float>(mouse.delta_x),
                                 static_cast<float>(-mouse.delta_y));
        });

        loop.set_render_callback([&](hz::f64 alpha) {
            HZ_UNUSED(alpha);

            renderer.begin_frame();

            auto [width, height] = renderer.framebuffer_size();
            float aspect = static_cast<float>(width) / static_cast<float>(height);

            glm::mat4 view = camera.view_matrix();
            glm::mat4 projection = camera.projection_matrix(aspect);

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

            textured_shader.bind();
            textured_shader.set_mat4("u_model", cube_model);
            textured_shader.set_mat4("u_view", view);
            textured_shader.set_mat4("u_projection", projection);
            textured_shader.set_int("u_texture", 0);
            textured_shader.set_int("u_use_texture", 1);
            cube.draw();

            // Physics falling cube (same texture as rotating cube)
            glm::vec3 phys_pos = physics.get_body_position(falling_cube);
            glm::quat phys_rot = physics.get_body_rotation(falling_cube);

            glm::mat4 phys_model = glm::mat4(1.0f);
            phys_model = glm::translate(phys_model, phys_pos);
            phys_model = phys_model * glm::mat4_cast(phys_rot);

            textured_shader.set_mat4("u_model", phys_model);
            textured_shader.set_int("u_use_texture", 1); // Use texture
            physics_cube_mesh.draw();

            // ========================================
            // 2. Render transparent objects
            // ========================================

            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDepthMask(GL_FALSE);

            glm::mat4 ground_model = glm::mat4(1.0f);
            grid_shader.bind();
            grid_shader.set_mat4("u_model", ground_model);
            grid_shader.set_mat4("u_view", view);
            grid_shader.set_mat4("u_projection", projection);
            grid_shader.set_vec3("u_camera_pos", camera.position());
            ground_plane.draw();

            glDepthMask(GL_TRUE);
            glDisable(GL_BLEND);

            // Render UI
            imgui.begin_frame();
            debug_overlay.draw(fps, frame_time, 2); // 2 bodies: ground + falling cube
            imgui.end_frame();

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
