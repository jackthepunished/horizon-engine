/**
 * @file main.cpp
 * @brief Sample FPS game demonstrating the Horizon Engine
 *
 * This example creates a window with an FPS camera, renders a grid plane,
 * a textured cube, a physics-simulated falling cube, and allows WASD movement.
 */

#include "editor_ui.hpp"
#include "engine/core/memory.hpp"
#include "engine/physics/physics_world.hpp"
#include "engine/scene/scene.hpp"

#include <backends/imgui_impl_opengl3.h>
#include <engine/animation/animator.hpp>
#include <engine/assets/asset_registry.hpp>
#include <engine/assets/cubemap.hpp>
#include <engine/assets/texture.hpp>
#include <engine/audio/audio_engine.hpp>
#include <engine/core/game_loop.hpp>
#include <engine/core/log.hpp>
#include <engine/platform/input.hpp>
#include <engine/platform/window.hpp>
#include <engine/renderer/camera.hpp>
#include <engine/renderer/grass.hpp>
#include <engine/renderer/ibl.hpp>
#include <engine/renderer/particle_system.hpp>
#include <engine/renderer/water.hpp>
#include <engine/renderer/mesh.hpp>
#include <engine/renderer/opengl/shader.hpp>
#include <engine/renderer/render_utils.hpp>
#include <engine/renderer/renderer.hpp>
#include <engine/renderer/terrain.hpp>
#include <engine/scene/components.hpp>
#include <engine/scene/scene_serializer.hpp>
#include <engine/ui/debug_overlay.hpp>
#include <engine/ui/imgui_layer.hpp>
#include <imgui.h>

#define GLFW_INCLUDE_NONE
#include <fstream>
#include <random>
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
        window_config.vsync = false; // Disable VSync for performance testing

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
        renderer.set_clear_color(0.7f, 0.8f, 1.0f); // Match horizon color

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

        // Terrain shader
        std::string terrain_vert = read_file("assets/shaders/terrain.vert");
        std::string terrain_frag = read_file("assets/shaders/terrain.frag");
        hz::gl::Shader terrain_shader(terrain_vert, terrain_frag);

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

        // ==========================================
        // Initialize IBL (Image-Based Lighting)
        // ==========================================
        hz::IBL ibl;
        bool ibl_ready = ibl.generate("assets/textures/skybox/qwantani_sunset_puresky_2k.hdr", 512);
        if (ibl_ready) {
            HZ_LOG_INFO("IBL initialized with Sunset HDR!");
        } else {
            HZ_LOG_WARN("IBL initialization failed, falling back to simple ambient");
        }

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

        // ==========================================
        // Initialize Scene (ECS)
        // ==========================================
        hz::Scene scene;

        // ==========================================
        // Initialize Physics
        // ==========================================
        hz::PhysicsWorld physics;
        physics.init();

        // Create static ground (invisible - just for collision)
        // Set to -5.5 so the top surface is at -5.0 (matching our terrain offset)
        physics.create_static_box(glm::vec3(0.0f, -5.5f, 0.0f), glm::vec3(100.0f, 0.5f, 100.0f));

        // Create dynamic falling cube (Removed for Showcase)
        // const glm::vec3 CUBE_SPAWN_POS(3.0f, 10.0f, -5.0f);
        // hz::PhysicsBodyID falling_cube =
        //     physics.create_dynamic_box(CUBE_SPAWN_POS, glm::vec3(0.5f), 1.0f);

        // Reset timer
        // float reset_timer = 0.0f;
        bool waiting_to_reset = false;

        HZ_LOG_INFO("Scene created with physics!");

        // ==========================================
        // Create Terrain
        // ==========================================
        hz::Terrain terrain;
        hz::TerrainConfig terrain_config;
        terrain_config.width = 125.0f; // Halved
        terrain_config.depth = 125.0f;
        terrain_config.max_height = 15.0f;    // Lower peaks for smoother look
        terrain_config.texture_scale = 40.0f; // More tiling
        terrain_config.resolution = 256;      // Higher resolution for smoothness
        HZ_LOG_INFO("Terrain generated: {}x{}", terrain_config.width, terrain_config.depth);
        terrain.generate_procedural(terrain_config);

        // Load terrain textures
        // Load terrain textures
        hz::TextureHandle terrain_albedo =
            assets.load_texture("assets/textures/terrain/pbr/albedo.jpg");
        hz::TextureHandle terrain_normal =
            assets.load_texture("assets/textures/terrain/pbr/normal.jpg");
        hz::TextureHandle terrain_roughness =
            assets.load_texture("assets/textures/terrain/pbr/roughness.jpg");
        hz::TextureHandle terrain_ao = assets.load_texture("assets/textures/terrain/pbr/ao.jpg");

        // ==========================================
        // Create 3D Grass
        // ==========================================
        hz::Grass grass;
        hz::GrassConfig grass_config;
        grass_config.blade_count = 50000; // Very dense grass
        grass_config.min_height = 0.8f;   // Taller grass
        grass_config.max_height = 1.5f;
        grass_config.wind_strength = 0.3f;
        grass_config.wind_speed = 1.5f;
        grass_config.blade_width = 1.2f; // Much wider for clump texture
        grass.generate(terrain, grass_config);
        HZ_LOG_INFO("Grass generated: {} blades", grass.blade_count());

        // Load grass shader and texture
        std::string grass_vert = read_file("assets/shaders/grass.vert");
        std::string grass_frag = read_file("assets/shaders/grass.frag");
        hz::gl::Shader grass_shader(grass_vert, grass_frag);
        hz::TextureHandle grass_blade_tex =
            assets.load_texture("assets/textures/terrain/grass/grass_blade.png");

        // ==========================================
        // Create Water
        // ==========================================
        hz::Water water;
        hz::WaterConfig water_config;
        water_config.size = 80.0f;
        water_config.height = -3.0f;  // Water level
        water_config.wave_strength = 0.2f;
        water_config.wave_speed = 0.8f;
        water_config.distortion_strength = 0.02f;
        water_config.transparency = 0.7f;
        water_config.water_color = glm::vec3(0.0f, 0.2f, 0.4f);         // Deep blue
        water_config.water_color_shallow = glm::vec3(0.0f, 0.4f, 0.6f); // Light blue
        water.init(water_config);
        HZ_LOG_INFO("Water initialized: size={}, height={}", water_config.size, water_config.height);

        // Load water shader
        std::string water_vert = read_file("assets/shaders/water.vert");
        std::string water_frag = read_file("assets/shaders/water.frag");
        hz::gl::Shader water_shader(water_vert, water_frag);

        // ==========================================
        // Create Particle System
        // ==========================================
        hz::ParticleSystem particles;
        
        // Fire emitter - at a campfire location
        auto fire_config = hz::ParticlePresets::fire();
        fire_config.position = glm::vec3(10.0f, terrain.get_height_at(10.0f, 10.0f) - 4.5f, 10.0f);
        fire_config.emit_rate = 80.0f;
        hz::u32 fire_emitter = particles.create_emitter(fire_config);
        
        // Smoke emitter - above fire
        auto smoke_config = hz::ParticlePresets::smoke();
        smoke_config.position = fire_config.position + glm::vec3(0.0f, 1.5f, 0.0f);
        smoke_config.emit_rate = 20.0f;
        hz::u32 smoke_emitter = particles.create_emitter(smoke_config);
        
        // Sparkles emitter - magic effect at another location
        auto sparkle_config = hz::ParticlePresets::sparkles();
        sparkle_config.position = glm::vec3(-15.0f, terrain.get_height_at(-15.0f, -10.0f) - 2.0f, -10.0f);
        hz::u32 sparkle_emitter = particles.create_emitter(sparkle_config);
        
        HZ_LOG_INFO("Particle system initialized with {} emitters", particles.emitter_count());
        HZ_UNUSED(fire_emitter);
        HZ_UNUSED(smoke_emitter);
        HZ_UNUSED(sparkle_emitter);

        // Load particle shader
        std::string particle_vert = read_file("assets/shaders/particle.vert");
        std::string particle_frag = read_file("assets/shaders/particle.frag");
        hz::gl::Shader particle_shader(particle_vert, particle_frag);

        // Load volumetric fog shader
        std::string volumetric_vert = read_file("assets/shaders/volumetric.vert");
        std::string volumetric_frag = read_file("assets/shaders/volumetric.frag");
        hz::gl::Shader volumetric_shader(volumetric_vert, volumetric_frag);
        
        // Volumetric fog settings
        struct VolumetricSettings {
            bool enabled = true;
            float fog_density = 0.005f;          // Reduced from 0.015 - less dense fog
            float fog_height_falloff = 0.15f;    // Faster falloff with height
            float fog_base_height = -5.0f;       // Fog stays lower
            glm::vec3 fog_color = glm::vec3(0.85f, 0.9f, 1.0f);  // Brighter fog color
            float scattering_coeff = 0.3f;       // Reduced from 0.5 - less light absorption
            float absorption_coeff = 0.005f;     // Reduced from 0.02 - less light blocking
            float god_ray_intensity = 1.5f;      // Increased god rays
            float god_ray_decay = 0.95f;
            int ray_march_steps = 32;
        } volumetric_settings;

        // ==========================================
        // Load Plant Model (Periwinkle)
        // ==========================================
        hz::Model plant_model =
            hz::Model::load_from_gltf("assets/models/vegetation/periwinkle_plant_2k.gltf");
        hz::TextureHandle plant_albedo =
            assets.load_texture("assets/models/vegetation/textures/periwinkle_plant_diff_2k.jpg");
        hz::TextureHandle plant_normal =
            assets.load_texture("assets/models/vegetation/textures/periwinkle_plant_nor_gl_2k.jpg");

        HZ_LOG_INFO("Loaded plant model: periwinkle_plant (valid={})", plant_model.is_valid());

        // Generate random plant matrices for instancing
        const int PLANT_COUNT = 500;
        std::vector<glm::mat4> plant_matrices;
        plant_matrices.reserve(PLANT_COUNT);

        std::mt19937 plant_rng(54321);
        std::uniform_real_distribution<float> plant_dist_x(-60.0f, 60.0f);
        std::uniform_real_distribution<float> plant_dist_z(-60.0f, 60.0f);

        for (int i = 0; i < PLANT_COUNT; ++i) {
            float x = plant_dist_x(plant_rng);
            float z = plant_dist_z(plant_rng);
            float y = terrain.get_height_at(x, z) - 4.5f; // Raise higher to prevent sinking

            glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(x, y, z));
            model = glm::scale(model, glm::vec3(2.0f)); // Scale up
            plant_matrices.push_back(model);
        }

        plant_model.setup_instancing(plant_matrices);
        HZ_LOG_INFO("Generated {} plant instances", plant_matrices.size());

        // ==========================================
        // Load Tree Model
        // ==========================================
        hz::Model tree_model = hz::Model::load_from_gltf(
            "assets/models/vegetation/island_tree_01/island_tree_01_2k.gltf");
        HZ_LOG_INFO("Loaded tree model (valid={})", tree_model.is_valid());

        // Generate random tree matrices
        const int TREE_COUNT = 10;
        std::vector<glm::mat4> tree_matrices;
        tree_matrices.reserve(TREE_COUNT);

        // Load tree texture
        hz::TextureHandle tree_albedo = assets.load_texture(
            "assets/models/vegetation/island_tree_01/textures/island_tree_01_diff_2k.jpg");

        std::mt19937 tree_rng(9999); // Different seed
        for (int i = 0; i < TREE_COUNT; ++i) {
            float x = plant_dist_x(tree_rng);
            float z = plant_dist_z(tree_rng);
            float y = terrain.get_height_at(x, z); // No offset

            // Do not spawn trees too close to center (clearance)
            if (x * x + z * z < 100.0f)
                continue;

            glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(x, y, z));
            // Trees are big, scale them up!
            float scale = 3.0f + (static_cast<float>(tree_rng() % 100) / 50.0f); // 3.0 - 5.0 scale
            model = glm::scale(model, glm::vec3(scale));
            float rot = static_cast<float>(tree_rng() % 360);
            model = glm::rotate(model, glm::radians(rot), glm::vec3(0.0f, 1.0f, 0.0f));

            tree_matrices.push_back(model);
        }
        tree_model.setup_instancing(tree_matrices);
        HZ_LOG_INFO("Generated {} tree instances", tree_matrices.size());

        // ==========================================
        // Setup Lighting
        // ==========================================
        hz::SceneLighting lighting;
        // Initial lighting (will be synced from editor settings in render loop)
        lighting.sun.direction = glm::normalize(glm::vec3(0.0f, -1.0f, 0.0f));  // Overhead
        lighting.sun.color = glm::vec3(1.0f, 0.98f, 0.95f);   // Bright white
        lighting.sun.intensity = 6.0f;                         // Strong
        lighting.ambient_light = glm::vec3(0.4f, 0.5f, 0.6f);  // Bright ambient

        // Point Light 0 - Warm orange (orbiting)
        lighting.point_lights.push_back({});
        lighting.point_lights[0].color = glm::vec3(1.0f, 0.6f, 0.2f);
        lighting.point_lights[0].intensity = 3.0f;
        lighting.point_lights[0].range = 25.0f;

        // Point Light 1 - Cool blue
        lighting.point_lights.push_back({});
        lighting.point_lights[1].position = glm::vec3(-15.0f, 5.0f, -15.0f);
        lighting.point_lights[1].color = glm::vec3(0.2f, 0.5f, 1.0f);
        lighting.point_lights[1].intensity = 2.5f;
        lighting.point_lights[1].range = 20.0f;

        // Point Light 2 - Green
        lighting.point_lights.push_back({});
        lighting.point_lights[2].position = glm::vec3(20.0f, 4.0f, 10.0f);
        lighting.point_lights[2].color = glm::vec3(0.3f, 1.0f, 0.4f);
        lighting.point_lights[2].intensity = 2.0f;
        lighting.point_lights[2].range = 18.0f;

        // Point Light 3 - Purple
        lighting.point_lights.push_back({});
        lighting.point_lights[3].position = glm::vec3(-25.0f, 6.0f, 20.0f);
        lighting.point_lights[3].color = glm::vec3(0.8f, 0.2f, 1.0f);
        lighting.point_lights[3].intensity = 2.5f;
        lighting.point_lights[3].range = 22.0f;

        // Point Light 4 - Cyan
        lighting.point_lights.push_back({});
        lighting.point_lights[4].position = glm::vec3(30.0f, 5.0f, -20.0f);
        lighting.point_lights[4].color = glm::vec3(0.0f, 1.0f, 1.0f);
        lighting.point_lights[4].intensity = 2.0f;
        lighting.point_lights[4].range = 20.0f;

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

        // ECS World (already initialized as 'scene')
        // hz::Scene scene; // Already exists

        // ==========================================
        // Load Demon Weapon (GLTF)
        // ==========================================
        hz::Entity weapon_entity{entt::null}; // Default constructor
        hz::Model weapon_model = hz::Model::load_from_gltf("assets/models/demon_weapon/scene.gltf");

        if (weapon_model.is_valid()) {
            weapon_entity = scene.create_entity();
            scene.registry().emplace<hz::TagComponent>(weapon_entity, "Demon Weapon");
            auto& tc = scene.registry().emplace<hz::TransformComponent>(weapon_entity);

            // "Left of the boxes" -> Boxes are at X ~ -5 to 5. Let's put it at -10.
            // "Mid-air" -> Y = 5.0
            tc.position = glm::vec3(-12.0f, 5.0f, 0.0f);
            tc.scale = glm::vec3(5.0f);
            // tc.rotation = glm::vec3(90.0f, 0.0f, 0.0f);

            auto& mc = scene.registry().emplace<hz::MeshComponent>(weapon_entity);
            mc.mesh_path = "assets/models/demon_weapon/scene.gltf";
            mc.albedo_color = glm::vec3(1.0f);
            mc.metallic = 0.5f;
            mc.roughness = 0.5f;

            // Setup Skeletal Animation
            if (weapon_model.has_skeleton()) {
                auto& anim = scene.registry().emplace<hz::AnimatorComponent>(weapon_entity);
                anim.skeleton = weapon_model.skeleton();

                // Play first animation if available
                if (!weapon_model.animations().empty()) {
                    anim.play(weapon_model.animations()[0]);
                }
            }
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

            // Update Animators
            auto view = scene.registry().view<hz::Entity>();
            view.each([&](auto entity) {
                if (auto* animator = scene.registry().try_get<hz::AnimatorComponent>(entity)) {
                    animator->update(dt_f);
                }
            });

            // Rotate Weapon
            if (scene.is_valid(weapon_entity)) {
                if (auto* tc = scene.registry().try_get<hz::TransformComponent>(weapon_entity)) {
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
                    hz::SceneSerializer serializer(scene);
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
                    hz::SceneSerializer serializer(scene);
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

            // Update particle system
            particles.update(dt_f);

            // Animate Point Lights
            float t = static_cast<float>(glfwGetTime());

            // Light 0 - Orbiting warm light (follows terrain height)
            float l0_x = 15.0f * cos(t * 0.3f);
            float l0_z = 15.0f * sin(t * 0.3f);
            float l0_y = terrain.get_height_at(l0_x, l0_z) + 3.0f;
            lighting.point_lights[0].position = glm::vec3(l0_x, l0_y, l0_z);

            // Light 1 - Slow float up/down
            float l1_y = terrain.get_height_at(-15.0f, -15.0f) + 4.0f + sin(t * 0.5f) * 2.0f;
            lighting.point_lights[1].position.y = l1_y;

            // Light 2 - Figure-8 pattern
            float l2_x = 20.0f + sin(t * 0.4f) * 10.0f;
            float l2_z = 10.0f + sin(t * 0.8f) * 8.0f;
            float l2_y = terrain.get_height_at(l2_x, l2_z) + 3.5f;
            lighting.point_lights[2].position = glm::vec3(l2_x, l2_y, l2_z);

            // Light 3 - Pulsing intensity
            lighting.point_lights[3].intensity = 2.0f + sin(t * 2.0f) * 1.0f;
            float l3_y = terrain.get_height_at(-25.0f, 20.0f) + 5.0f;
            lighting.point_lights[3].position.y = l3_y;

            // Light 4 - Reverse orbit
            float l4_x = 30.0f + 10.0f * cos(-t * 0.25f);
            float l4_z = -20.0f + 10.0f * sin(-t * 0.25f);
            float l4_y = terrain.get_height_at(l4_x, l4_z) + 4.0f;
            lighting.point_lights[4].position = glm::vec3(l4_x, l4_y, l4_z);

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

            renderer.set_clear_color(settings.clear_color.r, settings.clear_color.g,
                                     settings.clear_color.b);
            renderer.begin_frame();

            auto [width, height] = renderer.framebuffer_size();
            float aspect = static_cast<float>(width) / static_cast<float>(height);

            glm::mat4 view = camera.view_matrix();
            glm::mat4 projection = camera.projection_matrix(aspect);

            // Update UBOs
            renderer.update_camera(view, projection, camera.position());
            renderer.update_scene(static_cast<float>(glfwGetTime()));

            // ========================================
            // 0. Geometry Pass (Render Normals/Depth)
            // ========================================
            renderer.begin_geometry_pass();
            geometry_shader.bind();
            geometry_shader.bind_uniform_block("CameraData", 0);
            // geometry_shader.set_mat4("u_view", view);       // Handled by UBO
            // geometry_shader.set_mat4("u_projection", projection); // Handled by UBO

            // DRAW SCENE (OPAQUE ONLY)
            // Ideally we wrap this in a lambda or function
            // DRAW SCENE (OPAQUE ONLY) - ECS
            // DRAW SCENE (OPAQUE ONLY) - ECS
            {
                auto view = scene.registry().view<hz::Entity>();
                view.each([&](auto entity) {
                    // HZ_LOG_INFO("Ren. ent: {}", entity.index); // Commented out to reduce spam
                    auto* tc = scene.registry().try_get<hz::TransformComponent>(entity);
                    auto* mc = scene.registry().try_get<hz::MeshComponent>(entity);

                    if (tc && mc) {
                        geometry_shader.set_mat4("u_model", tc->get_transform());
                        if (mc->mesh_path == "cube") {
                            cube.draw();
                        } else if (mc->mesh_path == "plane") {
                            ground_cube.draw();
                        }
                    }
                });
            }
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
            // Draw Objects that cast shadows (ECS)
            {
                auto view = scene.registry().view<hz::Entity>();
                view.each([&](auto entity) {
                    auto* tc = scene.registry().try_get<hz::TransformComponent>(entity);
                    auto* mc = scene.registry().try_get<hz::MeshComponent>(entity);

                    if (tc && mc) {
                        depth_shader.set_mat4("u_model", tc->get_transform());
                        if (mc->mesh_path == "cube") {
                            cube.draw();
                        } else if (mc->mesh_path == "plane") {
                            ground_cube.draw();
                        }
                    }
                });
            }

            // Terrain shadow
            glm::mat4 terrain_model_shadow =
                glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -5.0f, 0.0f));
            depth_shader.set_mat4("u_model", terrain_model_shadow);
            terrain.draw();

            renderer.end_shadow_pass();

            // ========================================
            // 2. Scene Lighting Pass (Render to HDR FBO)
            // ========================================
            renderer.begin_scene_pass();

            // Sync editor settings to lighting struct
            lighting.sun.direction = glm::normalize(settings.sun_direction);
            lighting.sun.color = settings.sun_color;
            lighting.sun.intensity = settings.sun_intensity;
            lighting.ambient_light = settings.ambient_color * settings.ambient_intensity;

            // Submit and Apply Lighting
            renderer.submit_lighting(lighting);
            renderer.bind_shadow_map(
                5); // Bind shadow map to unit 5 to avoid collision with material textures

            // Bind SSAO texture manually (since we get ID)
            glActiveTexture(GL_TEXTURE0 + 6);
            glBindTexture(GL_TEXTURE_2D, renderer.get_ssao_blur_texture_id());

            pbr_shader.bind();
            renderer.apply_lighting(pbr_shader); // Apply light uniforms (Shadow only)
            // pbr_shader.set_mat4("u_view_projection", projection * view); // UBO
            // pbr_shader.set_vec3("u_view_pos", camera.position()); // UBO
            pbr_shader.set_mat4("u_light_space_matrix", light_space_matrix);
            // pbr_shader.set_vec2("u_viewport_size", glm::vec2(width, height)); // UBO

            // Set Texture Slots (Must match what we bind below)
            pbr_shader.set_int("u_albedo_map", 0);
            pbr_shader.set_int("u_normal_map", 1);
            pbr_shader.set_int("u_metallic_map", 2);
            pbr_shader.set_int("u_roughness_map", 3);
            pbr_shader.set_int("u_ao_map", 4);
            pbr_shader.set_int("u_shadow_map", 5); // Shadow map at 5

            // IBL Textures
            pbr_shader.set_int("u_irradiance_map", 7);
            pbr_shader.set_int("u_prefilter_map", 8);
            pbr_shader.set_int("u_brdf_lut", 9);
            pbr_shader.set_bool("u_use_ibl", ibl_ready);

            // Bind IBL textures
            if (ibl_ready) {
                ibl.bind(7, 8, 9);
            }

            pbr_shader.set_bool("u_use_normal_map", true);
            pbr_shader.set_bool("u_use_metallic_map", false);
            pbr_shader.set_bool("u_use_roughness_map", false);
            pbr_shader.set_bool("u_use_ao_map", false);
            pbr_shader.set_bool("u_has_animations", false); // Default to false for static objects
            // pbr_shader.set_vec3("u_specular_color", glm::vec3(0.5f)); // PBR doesn't use specular
            // color like Phong pbr_shader.set_float("u_shininess", 32.0f);

            // Render ECS Scene
            // Render ECS Scene
            {
                auto view = scene.registry().view<hz::Entity>();
                view.each([&](auto entity) {
                    auto* tc = scene.registry().try_get<hz::TransformComponent>(entity);
                    auto* mc = scene.registry().try_get<hz::MeshComponent>(entity);
                    if (!tc || !mc) {
                        return;
                    }

                    pbr_shader.set_mat4("u_model", tc->get_transform());
                    pbr_shader.set_vec3("u_albedo", mc->albedo_color);
                    pbr_shader.set_float("u_metallic", mc->metallic);
                    pbr_shader.set_float("u_roughness", mc->roughness);
                    pbr_shader.set_float("u_ao", 1.0f);

                    auto apply_wood = [&]() {
                        if (auto* t = assets.get_texture(tex_handle))
                            t->bind(0);
                        if (auto* t = assets.get_texture(normal_map_handle))
                            t->bind(1);
                        pbr_shader.set_bool("u_use_metallic_map", false);
                        pbr_shader.set_bool("u_use_roughness_map", false);
                        pbr_shader.set_bool("u_use_ao_map", false);
                        pbr_shader.set_bool("u_use_textures", true);
                        pbr_shader.set_bool("u_use_normal_map", true);
                    };

                    auto apply_brick = [&]() {
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
                        pbr_shader.set_bool("u_use_textures", true);
                        pbr_shader.set_bool("u_use_normal_map", true);
                        pbr_shader.set_bool("u_use_metallic_map", true);
                        pbr_shader.set_bool("u_use_roughness_map", true);
                        pbr_shader.set_bool("u_use_ao_map", true);
                        if (mc->mesh_path == "plane") {
                            pbr_shader.set_float("u_uv_scale", 100.0f);
                        }
                    };

                    auto apply_metal = [&]() {
                        if (auto* t = assets.get_texture(metal_albedo_handle))
                            t->bind(0);
                        pbr_shader.set_bool("u_use_textures", true);
                        pbr_shader.set_bool("u_use_metallic_map", false);
                        pbr_shader.set_bool("u_use_roughness_map", false);
                        pbr_shader.set_bool("u_use_normal_map", false);
                    };

                    const auto& albedo_path = mc->albedo_path;
                    if (albedo_path == "wood") {
                        apply_wood();
                    } else if (albedo_path == "brick") {
                        apply_brick();
                    } else if (albedo_path == "metal") {
                        apply_metal();
                    } else {
                        pbr_shader.set_bool("u_use_textures", false);
                        pbr_shader.set_bool("u_use_normal_map", false);
                    }

                    if (mc->mesh_path == "cube") {
                        cube.draw();
                    } else if (mc->mesh_path == "plane") {
                        ground_cube.draw();
                    } else if (mc->mesh_path == "assets/models/demon_weapon/scene.gltf") {
                        auto* anim = scene.registry().try_get<hz::AnimatorComponent>(entity);
                        const bool animated =
                            anim && anim->skeleton && !anim->bone_transforms.empty();
                        pbr_shader.set_bool("u_has_animations", animated);
                        if (animated) {
                            pbr_shader.set_mat4_array("u_bone_matrices",
                                                      anim->bone_transforms.data(),
                                                      static_cast<hz::u32>(anim->bone_transforms.size()));
                        }

                        if (weapon_model.is_valid()) {
                            weapon_model.draw();
                        }
                    } else {
                        pbr_shader.set_bool("u_has_animations", false);
                    }

                    pbr_shader.set_float("u_uv_scale", 1.0f);
                });
            }

            // ========================================
            // Render Terrain
            // ========================================
            terrain_shader.bind();
            terrain_shader.bind_uniform_block("CameraData", 0);
            terrain_shader.bind_uniform_block("SceneData", 1);
            // Lower the terrain by 5 units to make it feel more grounded
            glm::mat4 terrain_model = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -5.0f, 0.0f));
            terrain_shader.set_mat4("u_model", terrain_model);
            terrain_shader.set_mat4("u_light_space_matrix", light_space_matrix);

            // Fog & Lighting handled by UBO
            // Point lights handled by UBO

            // Material
            terrain_shader.set_float("u_roughness", 0.8f);
            terrain_shader.set_float("u_metallic", 0.0f);

            // Disable splatmap blending (no splatmap texture available)
            terrain_shader.set_bool("u_use_splatmap", false);

            // Texture slots for terrain
            terrain_shader.set_int("u_texture0", 0); // Albedo texture
            terrain_shader.set_int("u_texture1", 0);
            terrain_shader.set_int("u_texture2", 0);
            terrain_shader.set_int("u_texture3", 0);
            terrain_shader.set_int("u_shadow_map", 5);

            // Normal maps - disabled for testing
            terrain_shader.set_int("u_normal0", 1);
            terrain_shader.set_int("u_normal1", 1);
            terrain_shader.set_int("u_normal2", 1);
            terrain_shader.set_int("u_normal3", 1);
            terrain_shader.set_bool("u_use_normal_maps", true);

            // PBR Maps
            terrain_shader.set_int("u_roughness_map", 2);
            terrain_shader.set_int("u_ao_map", 3);
            terrain_shader.set_bool("u_use_pbr_maps", true);

            // IBL for terrain - disabled (causes issues with current normal setup)
            terrain_shader.set_int("u_irradiance_map", 7);
            terrain_shader.set_int("u_prefilter_map", 8);
            terrain_shader.set_int("u_brdf_lut", 9);
            terrain_shader.set_bool("u_use_ibl", false);

            // if (ibl_ready) {
            //     ibl.bind(7, 8, 9);
            // }

            // Bind shadow map
            renderer.bind_shadow_map(5);

            // Bind terrain textures
            // Slot 0: Rock albedo
            if (auto* t = assets.get_texture(terrain_albedo))
                t->bind(0);
            // Slot 1: Rock normal
            if (auto* t = assets.get_texture(terrain_normal))
                t->bind(1);
            // Slot 2: Roughness
            if (auto* t = assets.get_texture(terrain_roughness))
                t->bind(2);
            // Slot 3: AO
            if (auto* t = assets.get_texture(terrain_ao))
                t->bind(3);

            // Debug: Disable face culling for terrain (test winding order)
            glDisable(GL_CULL_FACE);
            terrain.draw();
            glEnable(GL_CULL_FACE);

            // ========================================
            // Render 3D Grass
            // ========================================
            grass_shader.bind();
            grass_shader.bind_uniform_block("CameraData", 0);
            grass_shader.bind_uniform_block("SceneData", 1);
            // u_view_projection & u_camera_pos replaced by UBO

            grass_shader.set_float("u_time", static_cast<float>(glfwGetTime()));
            grass_shader.set_float("u_wind_strength", grass_config.wind_strength);
            grass_shader.set_float("u_wind_speed", grass_config.wind_speed);
            grass_shader.set_float("u_blade_width", grass_config.blade_width);

            // Grass colors
            grass_shader.set_vec3("u_base_color", glm::vec3(0.2f, 0.4f, 0.1f)); // Dark green base
            grass_shader.set_vec3("u_tip_color", glm::vec3(0.4f, 0.7f, 0.2f));  // Light green tips

            // Lighting & Fog handled by UBO
            // grass_shader.set_vec3("u_view_pos", camera.position());

            // Bind grass blade texture
            grass_shader.set_int("u_grass_texture", 0);
            if (auto* t = assets.get_texture(grass_blade_tex))
                t->bind(0);

            // Enable alpha blending for grass transparency
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDisable(GL_CULL_FACE); // Grass is double-sided

            grass.draw(static_cast<float>(glfwGetTime()));

            glDisable(GL_BLEND);
            glEnable(GL_CULL_FACE);

            // ========================================
            // Render Plants (Periwinkle)
            // ========================================
            pbr_shader.bind();
            // pbr_shader.set_mat4("u_view", view); // UBO
            // pbr_shader.set_mat4("u_projection", projection); // UBO
            pbr_shader.set_mat4("u_light_space_matrix", light_space_matrix);
            // pbr_shader.set_vec3("u_view_pos", camera.position()); // UBO

            // Lighting handled by UBO

            // Bind Plant Textures
            pbr_shader.set_bool("u_use_textures", true);
            pbr_shader.set_int("u_albedo_map", 0);
            pbr_shader.set_int("u_normal_map", 1);
            pbr_shader.set_int("u_roughness_map", 0); // Point to valid texture to be safe
            pbr_shader.set_int("u_metallic_map", 0);
            pbr_shader.set_int("u_ao_map", 0);
            pbr_shader.set_bool("u_use_normal_map", true);

            // Use uniform for roughness/metallic (ARM texture packing not supported yet)
            pbr_shader.set_bool("u_use_roughness_map", false);
            pbr_shader.set_bool("u_use_metallic_map", false);
            pbr_shader.set_bool("u_use_ao_map", false);
            pbr_shader.set_float("u_roughness", 0.8f);
            pbr_shader.set_float("u_metallic", 0.0f);
            pbr_shader.set_bool("u_use_ibl", false);

            if (auto* t = assets.get_texture(plant_albedo))
                t->bind(0);
            if (auto* t = assets.get_texture(plant_normal))
                t->bind(1);

            // Disable culling for plants
            glDisable(GL_CULL_FACE);

            // Enable instancing in shader
            pbr_shader.set_bool("u_instanced", true);

            // Single draw call for all plants
            plant_model.draw_instanced(static_cast<unsigned int>(plant_matrices.size()));

            pbr_shader.set_bool("u_instanced", false);

            glEnable(GL_CULL_FACE);

            // ========================================
            // Render Trees
            // ========================================
            // Trees have leaves which are double-sided
            glDisable(GL_CULL_FACE);
            pbr_shader.set_bool("u_instanced", true);

            // Bind Tree Textures
            pbr_shader.set_bool("u_use_textures", true);
            if (auto* t = assets.get_texture(tree_albedo)) {
                t->bind(0); // Albedo at slot 0
            }

            // Draw Instanced Trees
            tree_model.draw_instanced(static_cast<unsigned int>(tree_matrices.size()));

            pbr_shader.set_bool("u_instanced", false);
            glEnable(GL_CULL_FACE);

            // ========================================
            // Render Water
            // ========================================
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            
            water_shader.bind();
            water_shader.bind_uniform_block("CameraData", 0);
            water_shader.bind_uniform_block("SceneData", 1);
            
            glm::mat4 water_model = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 0.0f));
            water_shader.set_mat4("u_model", water_model);
            // u_time comes from SceneData UBO
            water_shader.set_float("u_wave_strength", water.config().wave_strength);
            water_shader.set_float("u_wave_speed", water.config().wave_speed);
            water_shader.set_float("u_distortion_strength", water.config().distortion_strength);
            water_shader.set_float("u_shine_damper", water.config().shine_damper);
            water_shader.set_float("u_reflectivity", water.config().reflectivity);
            water_shader.set_float("u_transparency", water.config().transparency);
            water_shader.set_float("u_depth_multiplier", water.config().depth_multiplier);
            water_shader.set_vec3("u_water_color", water.config().water_color);
            water_shader.set_vec3("u_water_color_shallow", water.config().water_color_shallow);
            
            // For now, bind scene texture as both reflection/refraction (simple effect)
            // Full implementation would require separate render passes
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, renderer.get_scene_texture_id());
            water_shader.set_int("u_reflection_texture", 0);
            water_shader.set_int("u_refraction_texture", 0);
            water_shader.set_int("u_dudv_map", 0);      // Reuse for now
            water_shader.set_int("u_normal_map", 0);    // Reuse for now
            water_shader.set_int("u_depth_texture", 0); // Reuse for now
            
            water.draw();
            glDisable(GL_BLEND);

            // ========================================
            // Render Particles
            // ========================================
            glEnable(GL_BLEND);
            glDepthMask(GL_FALSE); // Don't write to depth buffer
            
            particle_shader.bind();
            particle_shader.bind_uniform_block("CameraData", 0);
            particle_shader.set_bool("u_use_texture", false);
            
            // Draw fire (additive blend)
            glBlendFunc(GL_SRC_ALPHA, GL_ONE); // Additive for fire
            particle_shader.set_bool("u_additive_blend", true);
            if (auto* emitter = particles.get_emitter(fire_emitter)) {
                emitter->draw();
            }
            if (auto* emitter = particles.get_emitter(sparkle_emitter)) {
                emitter->draw();
            }
            
            // Draw smoke (normal blend)
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            particle_shader.set_bool("u_additive_blend", false);
            if (auto* emitter = particles.get_emitter(smoke_emitter)) {
                emitter->draw();
            }
            
            glDepthMask(GL_TRUE);
            glDisable(GL_BLEND);

            // ========================================
            // Render Skybox (last, with depth = 1.0)
            // ========================================
            glDepthFunc(GL_LEQUAL);  // Skybox passes where depth == 1.0
            glDisable(GL_CULL_FACE); // We're inside the sphere, don't cull
            skybox_shader.bind();

            // FPS Counter
            static double last_time = 0.0;
            static int frame_count = 0;
            double current_time = glfwGetTime();
            frame_count++;
            if (current_time - last_time >= 1.0) {
                HZ_LOG_INFO("FPS: {} ({:.2f} ms)", frame_count, 1000.0 / frame_count);
                frame_count = 0;
                last_time = current_time;
            }
            skybox_shader.set_mat4("u_view", view);
            skybox_shader.set_mat4("u_projection", projection);

            // Sunset sky texture
            skybox_shader.set_bool("u_use_texture", true);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_CUBE_MAP,
                          ibl.environment_map()); // Use the cubemap generated by IBL
            skybox_shader.set_int("u_skybox_map", 0);

            skybox_cube.draw();
            glEnable(GL_CULL_FACE); // Restore culling
            glDepthFunc(GL_LESS);   // Restore default

            // ========================================
            // Selection Outline Pass
            // ========================================
            if (editor.has_selection()) {
                hz::Entity selected = editor.selected_entity();
                auto* tc = scene.registry().try_get<hz::TransformComponent>(selected);
                auto* mc = scene.registry().try_get<hz::MeshComponent>(selected);

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
            // 3. Volumetric Fog / God Rays Pass
            // ========================================
            if (volumetric_settings.enabled) {
                volumetric_shader.bind();
                
                // Camera uniforms
                glm::mat4 inv_view_proj = glm::inverse(projection * view);
                volumetric_shader.set_mat4("u_inverse_view_projection", inv_view_proj);
                volumetric_shader.set_vec3("u_camera_pos", camera.position());
                volumetric_shader.set_float("u_near_plane", 0.1f);
                volumetric_shader.set_float("u_far_plane", 1000.0f);
                
                // Light uniforms
                glm::vec3 light_dir = -glm::normalize(lighting.sun.direction);
                volumetric_shader.set_vec3("u_light_dir", light_dir);
                volumetric_shader.set_vec3("u_light_color", lighting.sun.color);
                volumetric_shader.set_float("u_light_intensity", lighting.sun.intensity);
                volumetric_shader.set_mat4("u_light_space_matrix", light_space_matrix);
                
                // Volumetric settings
                volumetric_shader.set_float("u_fog_density", volumetric_settings.fog_density);
                volumetric_shader.set_float("u_fog_height_falloff", volumetric_settings.fog_height_falloff);
                volumetric_shader.set_float("u_fog_base_height", volumetric_settings.fog_base_height);
                volumetric_shader.set_vec3("u_fog_color", volumetric_settings.fog_color);
                volumetric_shader.set_float("u_scattering_coeff", volumetric_settings.scattering_coeff);
                volumetric_shader.set_float("u_absorption_coeff", volumetric_settings.absorption_coeff);
                
                // God rays settings
                volumetric_shader.set_float("u_god_ray_intensity", volumetric_settings.god_ray_intensity);
                volumetric_shader.set_float("u_god_ray_decay", volumetric_settings.god_ray_decay);
                volumetric_shader.set_int("u_ray_march_steps", volumetric_settings.ray_march_steps);
                
                // Time for noise animation
                volumetric_shader.set_float("u_time", static_cast<float>(glfwGetTime()));
                
                renderer.render_volumetric(volumetric_shader);
            }

            // ========================================
            // 4. Bloom Pass (Extract + Blur)
            // ========================================
            renderer.render_bloom(bloom_extract_shader, blur_shader, 0.8f, 5);

            // ========================================
            // 5. Post-Process Pass (HDR + Bloom -> Screen)
            // ========================================
            hdr_shader.bind();
            glActiveTexture(GL_TEXTURE0 + 1);
            glBindTexture(GL_TEXTURE_2D, renderer.get_bloom_texture_id());
            renderer.render_post_process(hdr_shader);

            // ========================================
            // UI Overlay (ImGui)
            // ========================================
            imgui.begin_frame();
            debug_overlay.draw(fps, frame_time, 2);
            scene.registry().ctx().erase<int>(); // Hack to ensure updated context if needed? No.
            editor.draw(scene, settings, fps, scene.entity_count());

            // Handle editor toolbar actions
            if (editor.should_add_cube()) {
                auto entity = scene.create_entity();
                scene.registry().emplace<hz::TagComponent>(entity, "New Cube");
                auto& tc = scene.registry().emplace<hz::TransformComponent>(entity);
                tc.position = camera.position() + camera.front() * 5.0f; // Spawn in front of camera
                auto& mc = scene.registry().emplace<hz::MeshComponent>(entity);
                mc.mesh_path = "cube";
                mc.albedo_color = glm::vec3(0.8f, 0.2f, 0.2f);
                mc.metallic = 0.5f;
                mc.roughness = 0.5f;
                editor.add_log("Added new cube entity");
            }

            if (editor.should_add_light()) {
                // Add a new point light to the scene
                // logic remains same as it pushes to vector
                // But wait, editor code for light component used add_component.
                // Main.cpp light logic uses `lighting` struct.
                // We should probably ADD an entity for the light too so it shows up in hierarchy!

                hz::PointLight new_light;
                new_light.position = camera.position() + camera.front() * 5.0f;
                new_light.color = glm::vec3(static_cast<float>(rand() % 100) / 100.0f,
                                            static_cast<float>(rand() % 100) / 100.0f,
                                            static_cast<float>(rand() % 100) / 100.0f);
                new_light.intensity = 2.0f + static_cast<float>(rand() % 30) / 10.0f;
                new_light.range = 15.0f + static_cast<float>(rand() % 20);

                // Add to lighting struct (renderer)
                lighting.point_lights.push_back(new_light);

                // Add to ECS for Editor
                auto entity = scene.create_entity();
                scene.registry().emplace<hz::TagComponent>(entity, "Point Light");
                auto& tc = scene.registry().emplace<hz::TransformComponent>(entity);
                tc.position = new_light.position;
                auto& lc = scene.registry().emplace<hz::LightComponent>(entity);
                lc.color = new_light.color;
                lc.intensity = new_light.intensity;
                lc.range = new_light.range;

                editor.add_log("Added point light entity");
            }

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
