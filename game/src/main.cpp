/**
 * @file main.cpp
 * @brief Horizon Engine - PBR Test Scene
 */

#include "engine/core/memory.hpp"
#include "engine/physics/physics_world.hpp"
#include "engine/scene/scene.hpp"

#include <backends/imgui_impl_opengl3.h>
#include <engine/animation/animator.hpp>
#include <engine/animation/ik_solver.hpp>
#include <engine/assets/asset_registry.hpp>
#include <engine/assets/cubemap.hpp>
#include <engine/assets/model.hpp>
#include <engine/assets/texture.hpp>
#include <engine/audio/audio_engine.hpp>
#include <engine/core/game_loop.hpp>
#include <engine/core/log.hpp>
#include <engine/platform/input.hpp>
#include <engine/platform/window.hpp>
#include <engine/renderer/camera.hpp>
#include <engine/renderer/debug_renderer.hpp>
#include <engine/renderer/deferred_renderer.hpp>
#include <engine/renderer/ibl.hpp>
#include <engine/renderer/mesh.hpp>
#include <engine/renderer/opengl/gl_context.hpp>
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

// =============================================================================
// Game Configuration Constants
// =============================================================================
namespace GameConfig {
// Window settings
constexpr int WINDOW_WIDTH = 1920;
constexpr int WINDOW_HEIGHT = 1080;
constexpr float ASPECT_RATIO = static_cast<float>(WINDOW_WIDTH) / static_cast<float>(WINDOW_HEIGHT);

// Player movement
constexpr float MOVEMENT_SPEED = 5.0f;
constexpr float MOUSE_SENSITIVITY = 0.1f;

// Player physics
constexpr float GRAVITY = -20.0f;
constexpr float JUMP_FORCE = 8.0f;
constexpr float GROUND_LEVEL = 1.6f;  // Player eye height (ground + 1.6m)
constexpr float CROUCH_HEIGHT = 0.8f; // Crouched eye height
constexpr float PLAYER_MASS = 80.0f;

// Player collider
constexpr float PLAYER_CAPSULE_RADIUS = 0.4f;
constexpr float PLAYER_CAPSULE_HALF_HEIGHT = 0.5f;

// Shooting
constexpr float RAYCAST_MAX_DISTANCE = 100.0f;
constexpr float IMPULSE_STRENGTH = 50.0f;

// VFX
constexpr float IMPACT_VFX_LIFETIME = 2.0f;
constexpr float IMPACT_VFX_SIZE = 0.2f;
} // namespace GameConfig

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
// Application
// ============================================================================

class Application {
public:
    void run() {
        hz::Log::init();
        HZ_LOG_INFO("Horizon Engine - PBR Test Scene (Deferred)");

        hz::MemoryContext::init();

        // Create window
        hz::WindowConfig window_config;
        window_config.title = "Horizon Engine - Deferred PBR Test";
        window_config.width = GameConfig::WINDOW_WIDTH;
        window_config.height = GameConfig::WINDOW_HEIGHT;
        window_config.vsync = false;

        hz::Window window(window_config);

        // Initialize OpenGL Context
        if (!hz::gl::init_context()) {
            HZ_FATAL("Failed to initialize OpenGL context");
            return;
        }

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
        input.bind_mouse_button(hz::InputManager::ACTION_PRIMARY_FIRE, GLFW_MOUSE_BUTTON_LEFT);

        // Initialize ImGui
        hz::ImGuiLayer imgui;
        imgui.init(window);

        // Create Deferred Renderer
        hz::DeferredRenderer renderer;
        if (!renderer.init(GameConfig::WINDOW_WIDTH, GameConfig::WINDOW_HEIGHT)) {
            HZ_FATAL("Failed to initialize Deferred Renderer");
            return;
        }

        // Initialize Audio
        hz::AudioSystem audio;
        audio.init();

        // Initialize Scene & Physics
        hz::Scene scene;
        hz::PhysicsWorld physics;
        physics.init();

        // Create Camera Entity (Player)
        auto player_entity = scene.create_entity();
        {
            auto& tc = scene.registry().emplace<hz::TransformComponent>(player_entity);
            tc.position = glm::vec3(0.0f, 1.6f, 6.0f);
            tc.rotation = glm::vec3(-12.0f, -90.0f, 0.0f); // Pitch, Yaw, Roll

            auto& cc = scene.registry().emplace<hz::CameraComponent>(player_entity);
            cc.primary = true;

            auto& tag = scene.registry().emplace<hz::TagComponent>(player_entity);
            tag.tag = "Player";

            auto& rb = scene.registry().emplace<hz::RigidBodyComponent>(player_entity);
            rb.type = hz::RigidBodyComponent::BodyType::Dynamic;
            rb.mass = GameConfig::PLAYER_MASS;
            rb.fixed_rotation = true;

            auto& capsule_collider =
                scene.registry().emplace<hz::CapsuleColliderComponent>(player_entity);
            capsule_collider.radius = GameConfig::PLAYER_CAPSULE_RADIUS;
            capsule_collider.half_height = GameConfig::PLAYER_CAPSULE_HALF_HEIGHT;
        }

        // Movement settings
        float movement_speed = GameConfig::MOVEMENT_SPEED;
        float mouse_sensitivity = GameConfig::MOUSE_SENSITIVITY;

        // FPS Player physics
        float player_vertical_velocity = 0.0f;
        const float gravity = GameConfig::GRAVITY;
        const float jump_force = GameConfig::JUMP_FORCE;
        const float ground_level = GameConfig::GROUND_LEVEL;
        const float crouch_height = GameConfig::CROUCH_HEIGHT;
        bool is_grounded = true;
        bool is_crouching = false;

        // ==========================================
        // Initialize IBL (Image-Based Lighting)
        // ==========================================
        hz::AssetRegistry assets;
        hz::IBL ibl;
        bool ibl_ready =
            ibl.generate("assets/textures/skybox/afrikaans_church_interior_4k.hdr", 1024);

        // IBL texture ID'lerini al
        GLuint irradiance_map = 0;
        GLuint prefilter_map = 0;
        GLuint brdf_lut = 0;
        GLuint environment_map = 0;

        if (ibl_ready) {
            HZ_LOG_INFO("IBL initialized with Afrikaans Church HDR!");
            irradiance_map = ibl.irradiance_map();
            prefilter_map = ibl.prefilter_map();
            brdf_lut = ibl.brdf_lut();
            environment_map = ibl.environment_map();
        } else {
            HZ_LOG_WARN("IBL initialization failed!");
        }

        // ==========================================
        // Setup PBR Grid
        // ==========================================
        hz::Mesh sphere_mesh = hz::Mesh::create_sphere(1.0f);
        hz::Mesh cube_mesh = hz::Mesh::create_cube(1.0f);

        // Create Sphere Mesh (shared resource)
        // In a real engine, this would be in AssetManager, but for now we construct it once here.
        // We can't easily share the VBO/VAO across MeshComponents without a resource system update,
        // so for now we will just use the "Primitive" mode of MeshComponent which creates its own
        // mesh on the fly, OR we can rely on the fact that MeshComponent::mesh_type = Primitive
        // uses internal helpers. let's use the Primitive type for now.

        int rows = 7;
        int cols = 7;
        float spacing = 2.5f;

        for (int row = 0; row < rows; ++row) {
            float metallic = (float)row / (float)rows;
            for (int col = 0; col < cols; ++col) {
                // Avoid ultra-mirror spheres so material reads more "normal"
                float roughness = glm::clamp((float)col / (float)cols, 0.25f, 1.0f);

                auto entity = scene.create_entity();

                // Transform
                auto& tc = scene.registry().emplace<hz::TransformComponent>(entity);
                tc.position =
                    glm::vec3((col - (cols / 2)) * spacing, 0.0f, (row - (rows / 2)) * spacing);
                tc.scale = glm::vec3(1.0f);
                tc.rotation = glm::vec3(0.0f);

                // Mesh & Material
                auto& mc = scene.registry().emplace<hz::MeshComponent>(entity);
                mc.mesh_type = hz::MeshComponent::MeshType::Primitive;
                mc.primitive_name = "sphere";
                mc.albedo_color = glm::vec3(1.0f, 0.0f, 0.0f); // Red
                mc.metallic = metallic;
                mc.roughness = roughness;

                auto& tag = scene.registry().emplace<hz::TagComponent>(entity);
                tag.tag = "GridSphere";
            }
        }

        // Lights
        std::vector<hz::GPUPointLight> point_lights;
        // Four corners
        point_lights.push_back({{-10.0f, 10.0f, 10.0f, 15.0f}, {300.0f, 300.0f, 300.0f, 5.0f}});
        point_lights.push_back({{10.0f, 10.0f, 10.0f, 15.0f}, {300.0f, 300.0f, 300.0f, 5.0f}});

        std::vector<hz::GPUSpotLight> spot_lights; // Empty for now

        // Geometry Shader for filling G-Buffer
        // We need to load it manually here to use in the loop, or does DeferredRenderer handle
        // binding? DeferredRenderer::begin_geometry_pass binds the FBO but NOT the shader
        // (usually). Since we are drawing manual objects, we must bind the shader.
        auto geometry_shader =
            std::make_unique<hz::gl::Shader>(read_file("assets/shaders/deferred/geometry.vert"),
                                             read_file("assets/shaders/deferred/geometry.frag"));

        auto shadow_shader =
            std::make_unique<hz::gl::Shader>(read_file("assets/shaders/deferred/shadow.vert"),
                                             read_file("assets/shaders/deferred/shadow.frag"));

        // Bind sampler units once (geometry pass)
        geometry_shader->bind();
        geometry_shader->set_int("u_AlbedoMap", 0);
        geometry_shader->set_int("u_NormalMap", 1);
        geometry_shader->set_int("u_MetallicRoughnessMap", 2);
        geometry_shader->set_int("u_AOMap", 3);
        geometry_shader->set_int("u_EmissionMap", 4);

        // ==========================================
        // Load a real GLTF model (textured)
        // ==========================================
        hz::Model test_model =
            hz::Model::load_from_gltf("assets/models/treasure_chest/treasure_chest_4k.gltf");
        if (!test_model.is_valid()) {
            HZ_LOG_WARN("Test model failed to load. Falling back to spheres only.");
        } else {
            HZ_LOG_INFO("Test model loaded successfully! Mesh count: {}", test_model.mesh_count());
        }

        // Texture parameters for GLTF models (no Y flip needed - GLTF uses OpenGL convention)
        hz::TextureParams albedo_params;
        albedo_params.srgb = true;    // Albedo is in sRGB
        albedo_params.flip_y = false; // GLTF UV convention
        albedo_params.generate_mipmaps = true;

        hz::TextureParams linear_params;
        linear_params.srgb = false;   // Normal/ARM maps are linear
        linear_params.flip_y = false; // GLTF UV convention
        linear_params.generate_mipmaps = true;

        // Treasure chest textures:
        // - diff = diffuse/albedo (sRGB)
        // - nor_gl = normal map (linear, OpenGL format - green channel = +Y)
        // - arm = AO(R)/Roughness(G)/Metallic(B) packed (linear)
        hz::Texture albedo_tex = hz::Texture::load_from_file(
            "assets/models/treasure_chest/textures/treasure_chest_diff_4k.jpg", albedo_params);
        hz::Texture normal_tex = hz::Texture::load_from_file(
            "assets/models/treasure_chest/textures/treasure_chest_nor_gl_4k.jpg", linear_params);
        hz::Texture arm_tex = hz::Texture::load_from_file(
            "assets/models/treasure_chest/textures/treasure_chest_arm_4k.jpg", linear_params);

        // Create Treasure Chest Entity
        if (test_model.is_valid()) {
            auto entity = scene.create_entity();
            auto& tc = scene.registry().emplace<hz::TransformComponent>(entity);
            tc.position = glm::vec3(0.0f, 5.0f, 0.0f); // Drop from height
            tc.rotation = glm::vec3(0.0f);             // Euler
            tc.scale = glm::vec3(1.0f);

            auto& mc = scene.registry().emplace<hz::MeshComponent>(entity);
            mc.mesh_type = hz::MeshComponent::MeshType::Model;
            // We'll use ID 0 to represent the treasure chest for this manual renderer
            mc.model.index = 0;

            auto& tag = scene.registry().emplace<hz::TagComponent>(entity);
            tag.tag = "TreasureChest";

            // Physics
            auto& rb = scene.registry().emplace<hz::RigidBodyComponent>(entity);
            rb.type = hz::RigidBodyComponent::BodyType::Dynamic;
            rb.mass = 10.0f;

            auto& bc = scene.registry().emplace<hz::BoxColliderComponent>(entity);
            bc.half_extents = glm::vec3(1.0f); // Approx size
        }

        // Create Floor Entity
        {
            auto entity = scene.create_entity();
            auto& tc = scene.registry().emplace<hz::TransformComponent>(entity);
            tc.position = glm::vec3(0.0f, -1.0f, 0.0f);
            tc.scale = glm::vec3(50.0f, 1.0f, 50.0f);

            auto& mc = scene.registry().emplace<hz::MeshComponent>(entity);
            mc.mesh_type = hz::MeshComponent::MeshType::Primitive;
            mc.primitive_name = "cube";
            mc.albedo_color = glm::vec3(0.5f);
            mc.metallic = 0.0f;
            mc.roughness = 0.8f;

            auto& tag = scene.registry().emplace<hz::TagComponent>(entity);
            tag.tag = "Floor";

            // Physics
            auto& rb = scene.registry().emplace<hz::RigidBodyComponent>(entity);
            rb.type = hz::RigidBodyComponent::BodyType::Static;

            auto& bc = scene.registry().emplace<hz::BoxColliderComponent>(entity);
            bc.half_extents = glm::vec3(50.0f, 1.0f, 50.0f);
        }

        // ==========================================
        // Game Loop
        // ==========================================
        hz::GameLoop loop;

        static bool tab_held = false;
        static bool show_grid = false;
        static bool show_model = true;
        static bool show_skeleton = false;
        static bool ik_enabled = false;
        static glm::vec3 ik_target_position = glm::vec3(6.0f, 1.0f, 0.5f);

        // Debug Renderer
        hz::DebugRenderer debug_renderer;
        debug_renderer.init();

        static glm::mat4 prev_view_projection = glm::mat4(1.0f);

        // ==========================================
        // Load Character Model (Joe)
        // ==========================================
        hz::Model character_model = hz::Model::load_from_fbx("assets/models/character.fbx");

        if (character_model.is_valid()) {
            HZ_LOG_INFO("Character model loaded! Animations: {}",
                        character_model.animations().size());

            auto entity = scene.create_entity();
            auto& tc = scene.registry().emplace<hz::TransformComponent>(entity);
            tc.position = glm::vec3(5.0f, 0.0f, 0.0f);
            tc.scale = glm::vec3(1.0f);
            tc.rotation = glm::vec3(0.0f, 180.0f, 0.0f); // Face towards camera

            auto& mc = scene.registry().emplace<hz::MeshComponent>(entity);
            mc.mesh_type = hz::MeshComponent::MeshType::Model;
            mc.model.index = 1; // Unique ID for character (0 is chest)

            if (character_model.has_skeleton()) {
                auto& ac = scene.registry().emplace<hz::AnimatorComponent>(entity);
                ac.skeleton = character_model.skeleton();
                if (!character_model.animations().empty()) {
                    // Play last animation (typically the actual animation, first is often T-Pose)
                    auto anim = character_model.animations().back();
                    HZ_LOG_INFO("Animation '{}': duration={}, ticks_per_sec={}, channels={}",
                                anim->name, anim->duration, anim->ticks_per_second,
                                anim->channels.size());
                    ac.play(anim, true); // Play animation, looping
                }
                HZ_LOG_INFO("Character has skeleton. Attached AnimatorComponent.");

                // Log bone names for IK setup
                HZ_LOG_INFO("Skeleton bones:");
                for (hz::u32 i = 0; i < character_model.skeleton()->bone_count(); ++i) {
                    const hz::Bone* bone =
                        character_model.skeleton()->get_bone(static_cast<hz::i32>(i));
                    if (bone) {
                        HZ_LOG_INFO("  Bone {}: {}", i, bone->name);
                    }
                }
            }
        } else {
            HZ_ERROR("Failed to load character model!");
        }

        loop.set_update_callback([&](hz::f64 dt) {
            float dt_f = static_cast<float>(dt);

            // ==============================================================================
            // Physics System
            // ==============================================================================
            physics.update(dt_f);

            // 1. Create Physics Bodies for new entities
            {
                auto view = scene.registry()
                                .view<hz::TransformComponent, hz::RigidBodyComponent,
                                      hz::BoxColliderComponent>();
                for (auto [entity, tc, rb, bc] : view.each()) {
                    if (!rb.created) {
                        hz::PhysicsBodyID body_id;
                        if (rb.type == hz::RigidBodyComponent::BodyType::Static) {
                            body_id = physics.create_static_box(tc.position, bc.half_extents);
                        } else {
                            body_id =
                                physics.create_dynamic_box(tc.position, bc.half_extents, rb.mass);
                        }

                        // Store the ID using type-safe helper (automatically cleaned up)
                        if (body_id.is_valid()) {
                            rb.set_body_id(new hz::PhysicsBodyID(body_id));
                            rb.created = true;
                        }
                    }
                }
            }

            // 2. Sync Physics -> ECS (for dynamic bodies)
            {
                auto view = scene.registry().view<hz::TransformComponent, hz::RigidBodyComponent>();
                for (auto [entity, tc, rb] : view.each()) {
                    if (rb.created && rb.runtime_body &&
                        rb.type == hz::RigidBodyComponent::BodyType::Dynamic) {
                        auto* body_id_ptr = rb.get_body_id<hz::PhysicsBodyID>();
                        if (body_id_ptr) {
                            tc.position = physics.get_body_position(*body_id_ptr);

                            // Convert Quaternion to Euler
                            glm::quat q = physics.get_body_rotation(*body_id_ptr);
                            tc.rotation = glm::degrees(glm::eulerAngles(q));
                        }
                    }
                }
            }

            // Camera Input
            // Find primary camera
            auto view = scene.registry().view<hz::TransformComponent, hz::CameraComponent>();
            for (auto [entity, tc, cc] : view.each()) {
                if (!cc.primary)
                    continue;

                // 1. Mouse Look (Update Rotation)
                if (window.is_cursor_captured()) {
                    const auto& mouse = input.mouse();
                    float x_offset = static_cast<float>(mouse.delta_x) * mouse_sensitivity;
                    float y_offset =
                        static_cast<float>(-mouse.delta_y) *
                        mouse_sensitivity; // Reversed since y-coordinates go from bottom to top

                    tc.rotation.y += x_offset; // Yaw
                    tc.rotation.x += y_offset; // Pitch

                    // Constrain pitch
                    if (tc.rotation.x > 89.0f)
                        tc.rotation.x = 89.0f;
                    if (tc.rotation.x < -89.0f)
                        tc.rotation.x = -89.0f;
                }

                // 2. Movement (Update Position)
                // Calculate front vector from rotation
                glm::vec3 front;
                front.x = cos(glm::radians(tc.rotation.y)) * cos(glm::radians(tc.rotation.x));
                front.y = sin(glm::radians(tc.rotation.x));
                front.z = sin(glm::radians(tc.rotation.y)) * cos(glm::radians(tc.rotation.x));
                front = glm::normalize(front);

                glm::vec3 right = glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f)));
                // glm::vec3 up = glm::normalize(glm::cross(right, front));

                // Track if player is moving for animation
                bool player_is_moving = false;

                if (input.is_action_active(hz::InputManager::ACTION_MOVE_FORWARD)) {
                    tc.position += front * movement_speed * dt_f;
                    player_is_moving = true;
                }
                if (input.is_action_active(hz::InputManager::ACTION_MOVE_BACKWARD)) {
                    tc.position -= front * movement_speed * dt_f;
                    player_is_moving = true;
                }
                if (input.is_action_active(hz::InputManager::ACTION_MOVE_LEFT)) {
                    tc.position -= right * movement_speed * dt_f;
                    player_is_moving = true;
                }
                if (input.is_action_active(hz::InputManager::ACTION_MOVE_RIGHT)) {
                    tc.position += right * movement_speed * dt_f;
                    player_is_moving = true;
                }
                // Jump (only when grounded)
                if (input.is_action_just_pressed(hz::InputManager::ACTION_JUMP) && is_grounded) {
                    player_vertical_velocity = jump_force;
                    is_grounded = false;
                }

                // Crouch toggle
                if (input.is_action_just_pressed(hz::InputManager::ACTION_CROUCH)) {
                    is_crouching = !is_crouching;
                }

                // Apply gravity
                player_vertical_velocity += gravity * dt_f;
                tc.position.y += player_vertical_velocity * dt_f;

                // Ground check
                float target_height = is_crouching ? crouch_height : ground_level;
                if (tc.position.y <= target_height) {
                    tc.position.y = target_height;
                    player_vertical_velocity = 0.0f;
                    is_grounded = true;
                }

                // Control character animation based on movement
                {
                    auto char_view =
                        scene.registry().view<hz::AnimatorComponent, hz::MeshComponent>();
                    for (auto [char_entity, animator, mesh] : char_view.each()) {
                        if (mesh.mesh_type == hz::MeshComponent::MeshType::Model &&
                            mesh.model.index == 1) {
                            if (player_is_moving) {
                                // Only start/resume animation if not already playing
                                if (animator.state == hz::AnimationState::Paused) {
                                    animator.resume();
                                } else if (animator.state == hz::AnimationState::Stopped) {
                                    // Only play from start when truly stopped
                                    if (animator.current_clip) {
                                        animator.play(animator.current_clip, true);
                                    }
                                }
                                // If already Playing, do nothing - let it continue
                            } else {
                                // Not moving - pause animation (preserves current_time)
                                animator.pause();
                            }
                        }
                    }
                }

                // Only process one primary camera
                break;
            }

            // Animation System
            {
                auto view = scene.registry().view<hz::AnimatorComponent>();
                for (auto [entity, animator] : view.each()) {
                    animator.update(dt_f);
                }
            }

            // Apply IK (after animation update)
            if (ik_enabled && character_model.has_skeleton()) {
                static hz::TwoBoneIK left_arm_ik;
                static hz::IKChain left_arm_chain;
                static bool ik_initialized = false;

                if (!ik_initialized) {
                    // Left arm: LeftArm(17) -> LeftForeArm(22) -> LeftHand(24)
                    left_arm_chain.bone_ids = {17, 22, 24};
                    left_arm_chain.calculate_length(*character_model.skeleton());
                    ik_initialized = true;
                }

                auto view =
                    scene.registry()
                        .view<hz::TransformComponent, hz::MeshComponent, hz::AnimatorComponent>();
                for (auto [entity, tc, mc, ac] : view.each()) {
                    if (mc.mesh_type == hz::MeshComponent::MeshType::Model && mc.model.index == 1) {
                        // Transform IK target from world space to model space
                        glm::mat4 model_inv = glm::inverse(tc.get_transform());
                        glm::vec3 local_target =
                            glm::vec3(model_inv * glm::vec4(ik_target_position, 1.0f));

                        // Set pole vector (elbow direction - behind character)
                        left_arm_ik.pole_vector = glm::vec3(0.0f, 0.0f, -50.0f);

                        // Apply IK
                        left_arm_ik.solve(*character_model.skeleton(), left_arm_chain, local_target,
                                          ac.bone_transforms);
                    }
                }
            }

            // Sync FPS Character with Camera (First-Person Arms)
            {
                // Find primary camera position and rotation
                glm::vec3 cam_pos{0.0f};
                glm::vec3 cam_rot{0.0f};
                bool found_camera = false;

                auto cam_view =
                    scene.registry().view<hz::TransformComponent, hz::CameraComponent>();
                for (auto [entity, tc, cc] : cam_view.each()) {
                    if (cc.primary) {
                        cam_pos = tc.position;
                        cam_rot = tc.rotation;
                        found_camera = true;
                        break;
                    }
                }

                if (found_camera) {
                    // Find character entity (model.index == 1)
                    auto char_view =
                        scene.registry().view<hz::TransformComponent, hz::MeshComponent>();
                    for (auto [entity, tc, mc] : char_view.each()) {
                        if (mc.mesh_type == hz::MeshComponent::MeshType::Model &&
                            mc.model.index == 1) {
                            // Position character at camera position
                            // Character origin is at feet, camera should be at eye level (~1.3m up)
                            // So we offset character down by that amount

                            // Calculate horizontal front direction (ignore pitch for character
                            // facing)
                            float yaw_rad = glm::radians(cam_rot.y);
                            glm::vec3 forward_flat(cos(yaw_rad), 0.0f, sin(yaw_rad));
                            forward_flat = glm::normalize(forward_flat);

                            // Character feet position: camera pos, but at ground level
                            // Eye height offset (character is ~140 units in FBX = 1.4m with
                            // scale 1.0) Camera at 1.6m, character eyes around 1.3-1.4m
                            tc.position = cam_pos;
                            tc.position.y -= 1.5f;              // Camera lower (more below eyes)
                            tc.position += forward_flat * 0.4f; // More forward

                            tc.rotation.y = cam_rot.y + 180.0f; // Character faces camera direction
                            tc.scale = glm::vec3(1.0f);         // Keep scale at 1
                            break;
                        }
                    }
                }
            }

            // Shooting
            if (input.is_action_just_pressed(hz::InputManager::ACTION_PRIMARY_FIRE)) {
                // Find player camera again (optimization: cache player entity or camera)
                // For now, re-query is fine for a single click
                auto cam_view =
                    scene.registry().view<hz::TransformComponent, hz::CameraComponent>();
                for (auto [entity, tc, cc] : cam_view.each()) {
                    if (cc.primary) {
                        // Raycast
                        glm::vec3 front;
                        front.x =
                            cos(glm::radians(tc.rotation.y)) * cos(glm::radians(tc.rotation.x));
                        front.y = sin(glm::radians(tc.rotation.x));
                        front.z =
                            sin(glm::radians(tc.rotation.y)) * cos(glm::radians(tc.rotation.x));
                        front = glm::normalize(front);

                        hz::RaycastHit hit =
                            physics.raycast(tc.position, front, GameConfig::RAYCAST_MAX_DISTANCE);
                        if (hit.hit) {
                            HZ_LOG_INFO("Raycast Hit! BodyID: {}",
                                        hit.body_id.id.GetIndexAndSequenceNumber());

                            // Apply impulse if dynamic
                            physics.apply_impulse(hit.body_id,
                                                  front * GameConfig::IMPULSE_STRENGTH);

                            // Basic Impact VFX (Red Sphere)
                            auto impact = scene.create_entity();
                            auto& tc_impact =
                                scene.registry().emplace<hz::TransformComponent>(impact);
                            tc_impact.position = hit.position;
                            tc_impact.scale = glm::vec3(GameConfig::IMPACT_VFX_SIZE);

                            auto& mc_impact = scene.registry().emplace<hz::MeshComponent>(impact);
                            mc_impact.primitive_name = "sphere";
                            mc_impact.albedo_color = glm::vec3(1.0f, 0.2f, 0.2f); // Red
                            mc_impact.metallic = 0.0f;
                            mc_impact.roughness = 0.8f;

                            scene.registry().emplace<hz::LifetimeComponent>(impact).time_remaining =
                                GameConfig::IMPACT_VFX_LIFETIME;
                        } else {
                            // Miss
                        }
                        break;
                    }
                }
            }

            // Lifetime System (VFX Cleanup)
            {
                auto view = scene.registry().view<hz::LifetimeComponent>();
                std::vector<hz::Entity> to_destroy;
                for (auto [entity, lifetime] : view.each()) {
                    lifetime.time_remaining -= dt_f;
                    if (lifetime.time_remaining <= 0.0f) {
                        to_destroy.push_back(entity);
                    }
                }
                for (auto e : to_destroy) {
                    scene.destroy_entity(e);
                }
            }

            if (input.is_action_just_pressed(hz::InputManager::ACTION_MENU)) {
                window.close();
            }

            // Toggle cursor
            if (glfwGetKey(window.native_handle(), GLFW_KEY_TAB) == GLFW_PRESS) {
                if (!tab_held) {
                    bool captured = window.is_cursor_captured();
                    window.set_cursor_captured(!captured);
                    tab_held = true;
                }
            } else {
                tab_held = false;
            }

            input.update();
        });

        loop.set_render_callback([&](hz::f64 alpha) {
            HZ_UNUSED(alpha);

            // 0. Shadow Pass (Directional shadow map)
            glm::vec3 sun_dir = glm::normalize(glm::vec3(-0.5f, -1.0f, -0.3f));
            glm::vec3 light_dir = -glm::normalize(sun_dir);
            glm::vec3 center = glm::vec3(0.0f);

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

            renderer.begin_shadow_pass(light_space);
            shadow_shader->bind();
            shadow_shader->set_mat4("u_LightSpaceMatrix", light_space);

            if (show_grid) {
                auto view = scene.registry().view<hz::TransformComponent, hz::MeshComponent>();
                for (auto [entity, tc, mc] : view.each()) {
                    if (mc.mesh_type == hz::MeshComponent::MeshType::Primitive &&
                        mc.primitive_name == "sphere") {
                        glm::mat4 model = tc.get_transform();
                        shadow_shader->set_mat4("u_Model", model);
                        sphere_mesh.draw();
                    }
                }
            }

            if (show_model && test_model.is_valid()) {
                // Find Treasure Chest Entity
                auto view = scene.registry().view<hz::TransformComponent, hz::MeshComponent>();
                for (auto [entity, tc, mc] : view.each()) {
                    if (mc.mesh_type == hz::MeshComponent::MeshType::Model && mc.model.index == 0) {
                        glDisable(GL_CULL_FACE);
                        glm::mat4 model = tc.get_transform();
                        shadow_shader->set_mat4("u_Model", model);
                        test_model.draw();
                        glEnable(GL_CULL_FACE);
                    }
                }
            }

            // Character Shadow
            if (character_model.is_valid()) {
                auto view =
                    scene.registry()
                        .view<hz::TransformComponent, hz::MeshComponent, hz::AnimatorComponent>();
                for (auto [entity, tc, mc, ac] : view.each()) {
                    glm::mat4 model = tc.get_transform();
                    shadow_shader->set_mat4("u_Model", model);

                    shadow_shader->set_bool("u_HasAnimation", true);
                    if (!ac.bone_transforms.empty()) {
                        shadow_shader->set_mat4_array("u_BoneMatrices", ac.bone_transforms.data(),
                                                      ac.bone_transforms.size());
                    }

                    character_model.draw();
                    shadow_shader->set_bool("u_HasAnimation", false);
                }
            }
            renderer.end_shadow_pass();

            // Find Primary Camera & Construct Temp Camera Object
            hz::Camera camera; // Default
            {
                auto view = scene.registry().view<hz::TransformComponent, hz::CameraComponent>();
                for (auto [entity, tc, cc] : view.each()) {
                    if (cc.primary) {
                        // Construct camera from ECS data
                        // Camera(pos, up, yaw, pitch)
                        camera = hz::Camera(tc.position, glm::vec3(0.0f, 1.0f, 0.0f), tc.rotation.y,
                                            tc.rotation.x);
                        camera.fov = cc.fov;
                        camera.near_plane = cc.near_plane;
                        camera.far_plane = cc.far_plane;
                        break;
                    }
                }
            }

            // 1. Geometry Pass (Fill G-Buffer)
            renderer.begin_geometry_pass(camera);

            geometry_shader->bind();
            geometry_shader->set_mat4("u_View", camera.view_matrix());
            {
                // Current Jittered Projection for rendering
                glm::mat4 proj = camera.projection_matrix(GameConfig::ASPECT_RATIO);
                glm::mat4 jittered_proj = renderer.get_taa_jittered_projection(proj);
                geometry_shader->set_mat4("u_Projection", jittered_proj);

                // Pass Previous Unjittered View-Projection for Velocity Calculation
                geometry_shader->set_mat4("u_PrevViewProjection", prev_view_projection);

                // Store current unjittered VP for next frame
                prev_view_projection = proj * camera.view_matrix();
            }

            // Render Entities (Spheres / Primitives)
            {
                auto view = scene.registry().view<hz::TransformComponent, hz::MeshComponent>();
                int visible_primitives = 0;
                for (auto [entity, tc, mc] : view.each()) {
                    if (mc.mesh_type == hz::MeshComponent::MeshType::Primitive) {
                        // Hack: If show_grid is false, skip entities that look like grid spheres?
                        // For now, let's just render everything to ensure VFX works.
                        // Ideally, we'd tag the grid spheres.

                        // Check if it's a GridSphere and if we should show it
                        bool is_grid_sphere = false;
                        if (auto* tag = scene.registry().try_get<hz::TagComponent>(entity)) {
                            if (tag->tag == "GridSphere") {
                                is_grid_sphere = true;
                            }
                        }

                        if (is_grid_sphere && !show_grid) {
                            continue;
                        }

                        visible_primitives++;

                        glm::mat4 model = tc.get_transform();
                        geometry_shader->set_mat4("u_Model", model);

                        geometry_shader->set_bool("u_UseAlbedoMap", false);
                        geometry_shader->set_bool("u_UseNormalMap", false);
                        geometry_shader->set_bool("u_UseMetallicRoughnessMap", false);
                        geometry_shader->set_bool("u_UseAOMap", false);
                        geometry_shader->set_bool("u_UseEmissionMap", false);

                        geometry_shader->set_vec3("u_AlbedoColor", mc.albedo_color);
                        geometry_shader->set_float("u_Metallic", mc.metallic);
                        geometry_shader->set_float("u_Roughness", mc.roughness);
                        geometry_shader->set_float("u_MaterialID", 1.0f); // Lit
                        geometry_shader->set_vec3("u_EmissionColor", glm::vec3(0.0f));
                        geometry_shader->set_float("u_EmissionStrength", 0.0f);

                        if (mc.primitive_name == "sphere") {
                            sphere_mesh.draw();
                        } else if (mc.primitive_name == "cube") {
                            cube_mesh.draw();
                        }
                    }
                }
            }

            // Render textured test model
            if (show_model && test_model.is_valid()) {
                auto view = scene.registry().view<hz::TransformComponent, hz::MeshComponent>();
                for (auto [entity, tc, mc] : view.each()) {
                    if (mc.mesh_type == hz::MeshComponent::MeshType::Model && mc.model.index == 0) {
                        glDisable(GL_CULL_FACE);
                        // Bind textures
                        albedo_tex.bind(0);
                        normal_tex.bind(1);
                        arm_tex.bind(2); // ARM = AO(R), Roughness(G), Metallic(B)

                        glm::mat4 model = tc.get_transform();
                        geometry_shader->set_mat4("u_Model", model);
                        geometry_shader->set_bool("u_UseAlbedoMap", albedo_tex.is_valid());
                        geometry_shader->set_bool("u_UseNormalMap", normal_tex.is_valid());
                        geometry_shader->set_bool("u_UseMetallicRoughnessMap", arm_tex.is_valid());
                        geometry_shader->set_bool("u_UseAOMap", false);
                        geometry_shader->set_bool("u_UseEmissionMap", false);

                        geometry_shader->set_vec3("u_AlbedoColor", glm::vec3(1.0f));
                        geometry_shader->set_float("u_Metallic", 1.0f);
                        geometry_shader->set_float("u_Roughness", 0.5f);
                        geometry_shader->set_float("u_MaterialID", 1.0f);
                        geometry_shader->set_vec3("u_EmissionColor", glm::vec3(0.0f));
                        geometry_shader->set_float("u_EmissionStrength", 0.0f);

                        test_model.draw();
                        glEnable(GL_CULL_FACE);
                    }
                }
            }

            // Render Character Model (Joe)
            // Use static check to ensure we only iterate if we actually have entities
            {
                auto view = scene.registry().view<hz::TransformComponent, hz::MeshComponent>();
                for (auto entity : view) {
                    auto& tc = view.get<hz::TransformComponent>(entity);
                    auto& mc = view.get<hz::MeshComponent>(entity);

                    if (mc.mesh_type == hz::MeshComponent::MeshType::Model && mc.model.index == 1) {
                        glm::mat4 model = tc.get_transform();
                        geometry_shader->set_mat4("u_Model", model);

                        bool has_anim = false;
                        if (auto* ac = scene.registry().try_get<hz::AnimatorComponent>(entity)) {
                            if (!ac->bone_transforms.empty()) {
                                has_anim = true;
                                geometry_shader->set_mat4_array("u_BoneMatrices",
                                                                ac->bone_transforms.data(),
                                                                ac->bone_transforms.size());
                            }
                        }

                        geometry_shader->set_bool("u_HasAnimation", has_anim);

                        // Bind FBX materials if available
                        bool has_albedo = false;
                        bool has_normal = false;
                        bool has_mr = false;
                        bool has_ao = false;
                        bool has_emission = false;

                        if (character_model.has_fbx_materials() &&
                            !character_model.fbx_materials().empty()) {
                            const auto& mat =
                                character_model.fbx_materials()[0]; // Use first material

                            if (mat.albedo_texture && mat.albedo_texture->is_valid()) {
                                mat.albedo_texture->bind(0);
                                has_albedo = true;
                            }
                            if (mat.normal_texture && mat.normal_texture->is_valid()) {
                                mat.normal_texture->bind(1);
                                has_normal = true;
                            }
                            if (mat.metallic_roughness_texture &&
                                mat.metallic_roughness_texture->is_valid()) {
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

                            // Set fallback colors from material
                            geometry_shader->set_vec3("u_AlbedoColor", mat.albedo_color);
                            geometry_shader->set_float("u_Metallic", mat.metallic);
                            geometry_shader->set_float("u_Roughness", mat.roughness);
                        } else {
                            // No FBX materials - use skin tone defaults
                            geometry_shader->set_vec3("u_AlbedoColor", glm::vec3(0.8f, 0.7f, 0.6f));
                            geometry_shader->set_float("u_Metallic", 0.0f);
                            geometry_shader->set_float("u_Roughness", 0.8f);
                        }

                        geometry_shader->set_bool("u_UseAlbedoMap", has_albedo);
                        geometry_shader->set_bool("u_UseNormalMap", has_normal);
                        geometry_shader->set_bool("u_UseMetallicRoughnessMap", has_mr);
                        geometry_shader->set_bool("u_UseAOMap", has_ao);
                        geometry_shader->set_bool("u_UseEmissionMap", has_emission);

                        glDisable(GL_CULL_FACE);
                        // glDisable(GL_DEPTH_TEST); // Removed: Let depth test work normally

                        // Use the LOCAL variable because we don't have an asset manager yet
                        character_model.draw();

                        // glEnable(GL_DEPTH_TEST); // Already enabled
                        glEnable(GL_CULL_FACE);

                        geometry_shader->set_bool("u_HasAnimation", false);
                    }
                }
            }

            renderer.end_geometry_pass();

            // 2. Shadows (Not set up yet)
            // renderer.render_shadows(glm::vec3(-0.5f, -1.0f, -0.3f));

            // 3. Lighting Pass
            glm::vec3 sun_color = glm::vec3(1.0f, 0.9f, 0.8f);

            renderer.execute_lighting_pass(camera, point_lights, spot_lights, sun_dir, sun_color,
                                           irradiance_map, prefilter_map, brdf_lut,
                                           environment_map);

            // 3.5 TAA Resolve (must run after lighting)
            renderer.execute_taa_pass();

            // 4. Post Process / Final Output
            renderer.render_to_screen();

            // 4.5 Debug Skeleton Rendering (after final output, before UI)
            if (show_skeleton && character_model.is_valid() && character_model.has_skeleton()) {
                auto view =
                    scene.registry()
                        .view<hz::TransformComponent, hz::MeshComponent, hz::AnimatorComponent>();
                for (auto [entity, tc, mc, ac] : view.each()) {
                    if (mc.mesh_type == hz::MeshComponent::MeshType::Model && mc.model.index == 1) {
                        debug_renderer.draw_skeleton(
                            *character_model.skeleton(), ac.bone_transforms, tc.get_transform(),
                            glm::vec3(0.0f, 1.0f, 0.0f), // Bone color (green)
                            glm::vec3(1.0f, 1.0f, 0.0f)  // Joint color (yellow)
                        );
                    }
                }

                // Get camera matrices for debug rendering
                glm::mat4 debug_vp =
                    camera.projection_matrix(GameConfig::ASPECT_RATIO) * camera.view_matrix();
                debug_renderer.render(debug_vp);
            }

            // IK Target Visualization
            if (ik_enabled) {
                // Draw IK target as a larger point with axes
                debug_renderer.draw_point(ik_target_position, 0.1f,
                                          glm::vec3(1.0f, 0.0f, 0.0f)); // Red
                debug_renderer.draw_axes(ik_target_position, 0.3f);

                // Render debug
                glm::mat4 debug_vp =
                    camera.projection_matrix(GameConfig::ASPECT_RATIO) * camera.view_matrix();
                debug_renderer.render(debug_vp);
            }

            // UI
            imgui.begin_frame();
            ImGui::Begin("PBR Test");
            ImGui::Text("Profiling: %.2f ms", 1000.0f / ImGui::GetIO().Framerate);
            ImGui::Checkbox("Show sphere grid", &show_grid);
            ImGui::Checkbox("Show test model (treasure_chest)", &show_model);
            ImGui::Checkbox("Show skeleton debug", &show_skeleton);
            ImGui::Checkbox("IK Demo", &ik_enabled);
            if (ik_enabled) {
                ImGui::DragFloat3("IK Target", &ik_target_position.x, 0.05f);
            }
            if (show_model) {
                // Find entity
                auto view = scene.registry().view<hz::TransformComponent, hz::MeshComponent>();
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
                        ImGui::DragFloat3("Char Scale", &tc.scale.x, 0.001f, 0.001f,
                                          2.0f); // Finer control
                    }
                }
            }
            // ImGui::Text("Tip: grid kapali iken modeli bulmak daha kolay.");
            ImGui::Text("Controls:");
            ImGui::BulletText("WASD: Move");
            ImGui::BulletText("Space: Jump");
            ImGui::BulletText("Left Click: Shoot (Physics Impulse)");
            ImGui::BulletText("Tab: Toggle Mouse Cursor");
            ImGui::End();

            // Crosshair
            // Always show crosshair if not captured (e.g. menu) or if we want a HUD
            // Ideally, show HUD only when playing? For now, always visible is fine.
            {
                auto* draw_list = ImGui::GetForegroundDrawList();
                ImVec2 center = ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f,
                                       ImGui::GetIO().DisplaySize.y * 0.5f);
                draw_list->AddCircleFilled(center, 3.0f,
                                           IM_COL32(255, 255, 255, 200));   // Simple dot crosshair
                draw_list->AddCircle(center, 4.0f, IM_COL32(0, 0, 0, 200)); // Outline
            }

            imgui.end_frame();

            // Swap Buffers (renderer.render_to_screen blits to backbuffer, but we need to swap)
            window.swap_buffers();
            glfwPollEvents();
        });

        HZ_LOG_INFO("Starting game loop...");
        loop.run();

        imgui.shutdown();
        renderer.shutdown();
        physics.shutdown();
        audio.shutdown();
        assets.clear();
        hz::MemoryContext::shutdown();
        hz::Log::shutdown();
    }
};

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
