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

#include <array>
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
#include <engine/rhi/rhi_command_list.hpp>
#include <engine/rhi/rhi_device.hpp>
#include <engine/rhi/rhi_resources.hpp>
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

    // RHI Core
    std::unique_ptr<hz::rhi::Device> m_device;
    std::unique_ptr<hz::rhi::Swapchain> m_swapchain;

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

    // Models & Meshes (optional because they are created during init)
    std::optional<hz::Mesh> m_plane_mesh;
    std::optional<hz::Mesh> m_sphere_mesh;
    std::optional<hz::Mesh> m_cube_mesh;
    std::optional<hz::Model> m_test_model;      // Treasure chest
    std::optional<hz::Model> m_character_model; // Character

    // Textures (optional)
    std::optional<hz::Texture> m_albedo_tex;
    std::optional<hz::Texture> m_normal_tex;
    std::optional<hz::Texture> m_arm_tex;

    // IBL textures
    hz::rhi::TextureView* m_irradiance_map{nullptr};
    hz::rhi::TextureView* m_prefilter_map{nullptr};
    hz::rhi::TextureView* m_brdf_lut{nullptr};
    hz::rhi::TextureView* m_environment_map{nullptr};

    // UI state
    bool m_show_grid{false};
    bool m_show_model{true};
    bool m_show_skeleton{false};
    glm::vec3 m_ik_target_position{6.0f, 1.0f, 0.5f};

    // Virtual frame synchronization
    static constexpr unsigned int APP_MAX_FRAMES_IN_FLIGHT = 2;
    unsigned int m_current_frame = 0;

    std::array<std::unique_ptr<hz::rhi::Semaphore>, 2> m_image_available_sems;
    std::array<std::unique_ptr<hz::rhi::Semaphore>, 2> m_render_finished_sems;
    std::array<std::unique_ptr<hz::rhi::Fence>, 2> m_frame_fences;

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
