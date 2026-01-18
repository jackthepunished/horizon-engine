#pragma once

/**
 * @file application.hpp
 * @brief Main application class that orchestrates all game systems
 */

#include "game_config.hpp"
#include "systems/animation_system.hpp"
#include "systems/character_system.hpp"
#include "systems/lifetime_system.hpp"
#include "systems/physics_system.hpp"
#include "systems/player_system.hpp"

#include <memory>
#include <optional>

#include <engine/assets/model.hpp>
#include <engine/assets/texture.hpp>
#include <engine/audio/audio_engine.hpp>
#include <engine/core/game_loop.hpp>
#include <engine/physics/physics_world.hpp>
#include <engine/platform/input.hpp>
#include <engine/platform/window.hpp>
#include <engine/renderer/debug_renderer.hpp>
#include <engine/renderer/deferred_renderer.hpp>
#include <engine/renderer/ibl.hpp>
#include <engine/renderer/mesh.hpp>
#include <engine/renderer/opengl/shader.hpp>
#include <engine/scene/scene.hpp>
#include <engine/ui/imgui_layer.hpp>

namespace game {

/**
 * @brief Main application class - owns all systems and resources
 */
class Application {
public:
    Application() = default;
    ~Application() = default;

    // Non-copyable, non-movable
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&) = delete;
    Application& operator=(Application&&) = delete;

    /**
     * @brief Initialize all systems
     * @return true on success
     */
    bool init();

    /**
     * @brief Run the main game loop
     */
    void run();

    /**
     * @brief Cleanup all resources
     */
    void shutdown();

private:
    // Core systems
    std::unique_ptr<hz::Window> m_window;
    std::unique_ptr<hz::InputManager> m_input;
    std::unique_ptr<hz::ImGuiLayer> m_imgui;
    std::unique_ptr<hz::DeferredRenderer> m_renderer;
    std::unique_ptr<hz::AudioSystem> m_audio;
    std::unique_ptr<hz::Scene> m_scene;
    std::unique_ptr<hz::PhysicsWorld> m_physics;
    std::unique_ptr<hz::DebugRenderer> m_debug_renderer;
    std::unique_ptr<hz::IBL> m_ibl;

    // Game systems
    PlayerSystem m_player_system;
    PhysicsSystem m_physics_system;
    AnimationSystem m_animation_system;
    CharacterSystem m_character_system;
    LifetimeSystem m_lifetime_system;

    // Shaders
    std::unique_ptr<hz::gl::Shader> m_geometry_shader;
    std::unique_ptr<hz::gl::Shader> m_shadow_shader;

    // Models & Meshes (optional because they are created during init)
    std::optional<hz::Mesh> m_sphere_mesh;
    std::optional<hz::Mesh> m_cube_mesh;
    std::optional<hz::Model> m_test_model;      // Treasure chest
    std::optional<hz::Model> m_character_model; // Character

    // Textures (optional)
    std::optional<hz::Texture> m_albedo_tex;
    std::optional<hz::Texture> m_normal_tex;
    std::optional<hz::Texture> m_arm_tex;

    // IBL textures
    GLuint m_irradiance_map{0};
    GLuint m_prefilter_map{0};
    GLuint m_brdf_lut{0};
    GLuint m_environment_map{0};

    // UI state
    bool m_show_grid{false};
    bool m_show_model{true};
    bool m_show_skeleton{false};
    glm::vec3 m_ik_target_position{6.0f, 1.0f, 0.5f};

    // Previous frame data for TAA
    glm::mat4 m_prev_view_projection{1.0f};

    // Input state
    bool m_tab_held{false};

    // Initialization helpers
    bool init_window();
    bool init_renderer();
    void init_input();
    void init_scene();
    void load_assets();
    void setup_scene_entities();

    // Game loop callbacks
    void on_update(float dt);
    void on_render(float alpha);

    // Helper to read shader files
    static std::string read_file(const std::string& path);
};

} // namespace game
