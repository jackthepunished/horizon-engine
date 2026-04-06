#pragma once

/**
 * @file editor_ui.hpp
 * @brief In-game Scene Hierarchy Editor & Property Inspector using ImGui
 *
 * Provides a Unity/Unreal-style editor overlay with:
 * - Scene hierarchy tree with search and entity icons
 * - Full property inspector for all component types
 * - Entity creation presets and component management
 * - Scene settings panel (environment, sun, post-processing)
 * - Console log panel
 * - Render stats overlay
 */

#include "engine/core/log.hpp"
#include "engine/renderer/deferred_renderer.hpp"
#include "engine/scene/components.hpp"
#include "engine/scene/scene.hpp"
#include "scene_settings.hpp"

#include <string>
#include <vector>

#include <imgui.h>

namespace game {

/**
 * @brief Full-featured in-game editor for ECS manipulation
 */
class EditorUI {
public:
    EditorUI() = default;
    ~EditorUI() = default;

    /**
     * @brief Apply a professional dark theme to ImGui
     *
     * Call once after ImGui context creation.
     */
    static void apply_dark_theme();

    /**
     * @brief Draw the entire editor UI
     * @param scene The Scene to edit
     * @param settings Scene settings to edit
     * @param fps Current FPS
     * @param entity_count Total entity count
     * @param render_stats Current frame render stats
     */
    void draw(hz::Scene& scene, SceneSettings& settings, float fps, size_t entity_count,
              const hz::RenderStats& render_stats);

    /**
     * @brief Add a log message to the console panel
     */
    void add_log(const std::string& message) {
        m_console_logs.push_back(message);
        if (m_console_logs.size() > 200)
            m_console_logs.erase(m_console_logs.begin());
        m_scroll_to_bottom = true;
    }

    // =========================================================================
    // Selection
    // =========================================================================

    [[nodiscard]] bool has_selection() const { return m_selected_entity != hz::Entity{entt::null}; }
    [[nodiscard]] hz::Entity selected_entity() const { return m_selected_entity; }
    void clear_selection() { m_selected_entity = entt::null; }

    // =========================================================================
    // Entity Preset Requests (polled by Application each frame)
    // =========================================================================

    [[nodiscard]] bool should_add_cube() {
        bool r = m_req_add_cube;
        m_req_add_cube = false;
        return r;
    }
    [[nodiscard]] bool should_add_sphere() {
        bool r = m_req_add_sphere;
        m_req_add_sphere = false;
        return r;
    }
    [[nodiscard]] bool should_add_light() {
        bool r = m_req_add_light;
        m_req_add_light = false;
        return r;
    }
    [[nodiscard]] bool should_add_camera() {
        bool r = m_req_add_camera;
        m_req_add_camera = false;
        return r;
    }
    [[nodiscard]] bool should_add_empty() {
        bool r = m_req_add_empty;
        m_req_add_empty = false;
        return r;
    }

    /**
     * @brief Check if a delete was requested (and get the entity to delete)
     */
    [[nodiscard]] bool should_delete_entity(hz::Entity& out_entity) {
        if (m_req_delete) {
            out_entity = m_req_delete_target;
            m_req_delete = false;
            m_req_delete_target = entt::null;
            return true;
        }
        return false;
    }

private:
    // =========================================================================
    // Panel Drawing
    // =========================================================================

    void draw_menu_bar();
    void draw_toolbar(float display_w, float menu_bar_height);
    void draw_hierarchy(hz::Scene& scene, float display_h, float top_offset, float width);
    void draw_inspector(hz::Scene& scene, float display_w, float display_h, float top_offset,
                        float width);
    void draw_scene_settings(SceneSettings& settings, float display_w, float display_h,
                             float top_offset, float width);
    void draw_stats(float fps, size_t entity_count, const hz::RenderStats& stats, float display_w,
                    float top_offset);
    void draw_console(float display_h, float top_offset, float width);

    // =========================================================================
    // Inspector Helpers
    // =========================================================================

    void draw_tag_section(hz::TagComponent& tag);
    void draw_transform_section(hz::TransformComponent& transform);
    void draw_mesh_section(hz::MeshComponent& mesh);
    void draw_light_section(hz::LightComponent& light);
    void draw_camera_section(hz::CameraComponent& cam);
    void draw_rigidbody_section(hz::RigidBodyComponent& rb);
    void draw_box_collider_section(hz::BoxColliderComponent& bc);
    void draw_sphere_collider_section(hz::SphereColliderComponent& sc);
    void draw_capsule_collider_section(hz::CapsuleColliderComponent& cc);
    void draw_ik_target_section(hz::IKTargetComponent& ik);

    /**
     * @brief Draw a collapsing component header with a remove option
     * @return true if the section is open
     */
    bool begin_component_section(const char* label, const char* id, bool can_remove,
                                 bool& out_remove_requested);

    void draw_add_component_popup(hz::Scene& scene, hz::Entity entity);

    // =========================================================================
    // Utility
    // =========================================================================

    /** @brief Get a display icon prefix for an entity based on its components */
    static const char* entity_icon(const entt::registry& reg, hz::Entity entity);

    // =========================================================================
    // State
    // =========================================================================

    hz::Entity m_selected_entity{entt::null};

    // Preset requests
    bool m_req_add_cube{false};
    bool m_req_add_sphere{false};
    bool m_req_add_light{false};
    bool m_req_add_camera{false};
    bool m_req_add_empty{false};

    // Delete request (deferred)
    bool m_req_delete{false};
    hz::Entity m_req_delete_target{entt::null};

    // Search
    char m_search_buffer[256]{};

    // Panel visibility
    bool m_show_hierarchy{true};
    bool m_show_inspector{true};
    bool m_show_settings{true};
    bool m_show_stats{true};
    bool m_show_console{true};
    bool m_show_toolbar{true};

    // Console
    std::vector<std::string> m_console_logs;
    bool m_scroll_to_bottom{false};
};

} // namespace game
