/**
 * @file main.cpp
 * @brief Sample FPS game demonstrating the Horizon Engine
 *
 * This example creates a window with an FPS camera, renders a grid plane,
 * and allows WASD movement with mouse look.
 */

#include <engine/core/game_loop.hpp>
#include <engine/core/log.hpp>
#include <engine/core/memory.hpp>
#include <engine/ecs/world.hpp>
#include <engine/platform/input.hpp>
#include <engine/platform/window.hpp>
#include <engine/renderer/camera.hpp>
#include <engine/renderer/mesh.hpp>
#include <engine/renderer/opengl/shader.hpp>
#include <engine/renderer/renderer.hpp>

#define GLFW_INCLUDE_NONE
#include <fstream>
#include <sstream>

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// ============================================================================
// Shader Source (embedded for simplicity)
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

class Application {
public:
    void run() {
        // Initialize logging
        hz::Log::init();
        HZ_LOG_INFO("Horizon Engine Sample Game starting...");

        // Initialize memory
        hz::MemoryContext::init();

        // Create window
        hz::WindowConfig window_config;
        window_config.title = "Horizon Engine - FPS Demo";
        window_config.width = 1280;
        window_config.height = 720;

        hz::Window window(window_config);

        // Setup input
        hz::InputManager input;
        input.attach(window);

        // Bind FPS controls
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
        hz::Camera camera(glm::vec3(0.0f, 2.0f, 5.0f));
        camera.movement_speed = 8.0f;

        // Create shader
        hz::gl::Shader grid_shader(GRID_VERTEX_SHADER, GRID_FRAGMENT_SHADER);

        // Create ground plane mesh
        hz::Mesh ground_plane = hz::Mesh::create_plane(100.0f, 100);

        HZ_LOG_INFO("Scene created: camera, shader, ground plane");

        // Capture mouse for FPS controls
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
        });

        loop.set_update_callback([&camera, &input](hz::f64 dt) {
            // Movement
            glm::vec3 move_dir{0.0f};

            if (input.is_action_active(hz::InputManager::ACTION_MOVE_FORWARD)) {
                move_dir.z = 1.0f; // Forward
            }
            if (input.is_action_active(hz::InputManager::ACTION_MOVE_BACKWARD)) {
                move_dir.z = -1.0f; // Backward
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

            // Sprint
            if (input.is_action_active(hz::InputManager::ACTION_SPRINT)) {
                camera.movement_speed = 16.0f;
            } else {
                camera.movement_speed = 8.0f;
            }

            camera.process_movement(move_dir, static_cast<float>(dt));

            // Mouse look
            const auto& mouse = input.mouse();
            camera.process_mouse(static_cast<float>(mouse.delta_x),
                                 static_cast<float>(-mouse.delta_y));
        });

        loop.set_render_callback([&renderer, &camera, &grid_shader, &ground_plane](hz::f64 alpha) {
            HZ_UNUSED(alpha);

            renderer.begin_frame();

            auto [width, height] = renderer.framebuffer_size();
            float aspect = static_cast<float>(width) / static_cast<float>(height);

            // Set up matrices
            glm::mat4 model = glm::mat4(1.0f);
            glm::mat4 view = camera.view_matrix();
            glm::mat4 projection = camera.projection_matrix(aspect);

            // Render ground plane
            grid_shader.bind();
            grid_shader.set_mat4("u_model", model);
            grid_shader.set_mat4("u_view", view);
            grid_shader.set_mat4("u_projection", projection);
            grid_shader.set_vec3("u_camera_pos", camera.position());

            // Enable blending for grid fade
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            ground_plane.draw();

            renderer.end_frame();
        });

        HZ_LOG_INFO("Starting game loop...");
        loop.run();

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
