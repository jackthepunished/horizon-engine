#pragma once

/**
 * @file editor_ui.hpp
 * @brief In-game ECS editor using ImGui
 */

#include "engine/core/log.hpp"
#include "engine/ecs/world.hpp"
#include "engine/scene/components.hpp"
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
     * @param world The ECS world to edit
     * @param settings Scene settings to edit
     * @param fps Current FPS
     * @param entity_count Total entity count
     */
    void draw(hz::World& world, SceneSettings& settings, float fps, size_t entity_count);

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
    [[nodiscard]] bool has_selection() const {
        return m_selected_entity.index != hz::Entity::INVALID_INDEX;
    }

    /**
     * @brief Get the selected entity
     */
    [[nodiscard]] hz::Entity selected_entity() const { return m_selected_entity; }

private:
    void draw_hierarchy(hz::World& world, float display_h, float menu_bar_height, float width);
    void draw_inspector(hz::World& world, float display_w, float display_h, float menu_bar_height,
                        float width);
    void draw_scene_settings(SceneSettings& settings, float display_w, float display_h,
                             float menu_bar_height, float width);
    void draw_stats(float fps, size_t entity_count, float display_w, float menu_bar_height);
    void draw_toolbar(float display_w, float menu_bar_height);
    void draw_console(float display_h, float menu_bar_height, float width);

    hz::Entity m_selected_entity{hz::Entity::INVALID_INDEX, 0};

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
