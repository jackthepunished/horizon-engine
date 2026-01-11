#pragma once

/**
 * @file editor_ui.hpp
 * @brief In-game ECS editor using ImGui
 */

#include "engine/core/log.hpp"
#include "engine/scene/components.hpp"
#include "engine/scene/scene.hpp"
#include "scene_settings.hpp"

#include <string>
#include <vector>

#include <imgui.h>

namespace game {

/**
 * @brief Simple in-game editor for ECS manipulation
 */
class EditorUI {
public:
    EditorUI() = default;
    ~EditorUI() = default;

    /**
     * @brief Draw the editor UI
     * @param scene The Scene to edit
     * @param settings Scene settings to edit
     * @param fps Current FPS
     * @param entity_count Total entity count
     */
    void draw(hz::Scene& scene, SceneSettings& settings, float fps, size_t entity_count);

    /**
     * @brief Add a log message to the console
     */
    void add_log(const std::string& message) {
        m_console_logs.push_back(message);
        if (m_console_logs.size() > 100)
            m_console_logs.erase(m_console_logs.begin());
        m_scroll_to_bottom = true;
    }

    /**
     * @brief Check if an entity is selected
     */
    [[nodiscard]] bool has_selection() const { return m_selected_entity != hz::Entity{entt::null}; }

    /**
     * @brief Get the selected entity
     */
    [[nodiscard]] hz::Entity selected_entity() const { return m_selected_entity; }

    /**
     * @brief Check if "Add Cube" was clicked
     */
    [[nodiscard]] bool should_add_cube() {
        bool result = m_add_cube_requested;
        m_add_cube_requested = false;
        return result;
    }

    /**
     * @brief Check if "Add Light" was clicked
     */
    [[nodiscard]] bool should_add_light() {
        bool result = m_add_light_requested;
        m_add_light_requested = false;
        return result;
    }

private:
    bool m_add_cube_requested{false};
    bool m_add_light_requested{false};
    void draw_hierarchy(hz::Scene& scene, float display_h, float menu_bar_height, float width);
    void draw_inspector(hz::Scene& scene, float display_w, float display_h, float menu_bar_height,
                        float width);
    void draw_scene_settings(SceneSettings& settings, float display_w, float display_h,
                             float menu_bar_height, float width);
    void draw_stats(float fps, size_t entity_count, float display_w, float menu_bar_height);
    void draw_toolbar(float display_w, float menu_bar_height);
    void draw_console(float display_h, float menu_bar_height, float width);

    hz::Entity m_selected_entity{entt::null};

    // Panel Visibility
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
