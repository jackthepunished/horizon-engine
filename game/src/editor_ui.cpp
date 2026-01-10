#include "editor_ui.hpp"

#include <imgui.h>

namespace game {

void EditorUI::draw(hz::World& world, SceneSettings& settings, float fps, size_t entity_count) {
    // Get viewport size for positioning
    ImGuiIO& io = ImGui::GetIO();
    float display_w = io.DisplaySize.x;
    float display_h = io.DisplaySize.y;

    // Panel dimensions
    const float hierarchy_width = 220.0f;
    const float inspector_width = 300.0f;
    const float menu_bar_height = 20.0f; // Approximate

    // Menu bar at top
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Save Scene (Todo)")) {
            }
            if (ImGui::MenuItem("Load Scene (Todo)")) {
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit (Todo)")) {
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Hierarchy", nullptr, &m_show_hierarchy);
            ImGui::MenuItem("Inspector", nullptr, &m_show_inspector);
            ImGui::MenuItem("Scene Settings", nullptr, &m_show_settings);
            ImGui::MenuItem("Stats", nullptr, &m_show_stats);
            ImGui::MenuItem("Console", nullptr, &m_show_console);
            ImGui::MenuItem("Toolbar", nullptr, &m_show_toolbar);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("Controls")) {
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    if (m_show_toolbar) {
        draw_toolbar(display_w, menu_bar_height);
    }

    // Adjust height for toolbar if visible
    float top_offset = menu_bar_height + (m_show_toolbar ? 40.0f : 0.0f);

    if (m_show_hierarchy) {
        draw_hierarchy(world, display_h, top_offset, hierarchy_width);
    }

    if (m_show_inspector) {
        draw_inspector(world, display_w, display_h, top_offset, inspector_width);
    }

    if (m_show_settings) {
        draw_scene_settings(settings, display_w, display_h, top_offset, inspector_width);
    }

    if (m_show_stats) {
        draw_stats(fps, entity_count, display_w, top_offset);
    }

    if (m_show_console) {
        draw_console(display_h, top_offset, hierarchy_width);
    }
}

void EditorUI::draw_toolbar(float display_w, float menu_bar_height) {
    ImGui::SetNextWindowPos(ImVec2(0, menu_bar_height));
    ImGui::SetNextWindowSize(ImVec2(display_w, 40.0f));
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoScrollWithMouse;

    // Style for toolbar
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));

    if (ImGui::Begin("Toolbar", &m_show_toolbar, flags)) {
        if (ImGui::Button("Save")) { /* Todo */
        }
        ImGui::SameLine();
        if (ImGui::Button("Load")) { /* Todo */
        }
        ImGui::SameLine();
        ImGui::Separator();
        ImGui::SameLine();
        if (ImGui::Button("Add Cube")) { /* Handler in main loop ideally */
        }
        ImGui::SameLine();
        if (ImGui::Button("Add Light")) { /* Handler in main loop ideally */
        }
    }
    ImGui::End();
    ImGui::PopStyleColor();
}

void EditorUI::draw_hierarchy(hz::World& world, float display_h, float top_offset, float width) {
    ImGui::SetNextWindowPos(ImVec2(0, top_offset), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(width, display_h - top_offset), ImGuiCond_Always);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;

    if (ImGui::Begin("Hierarchy", &m_show_hierarchy, flags)) {
        world.each_entity([&](hz::Entity entity) {
            std::string name = "Entity " + std::to_string(entity.index);
            if (auto* tag = world.get_component<hz::TagComponent>(entity)) {
                name = tag->tag;
            }

            bool is_selected = (m_selected_entity.index == entity.index);
            if (ImGui::Selectable(name.c_str(), is_selected)) {
                m_selected_entity = entity;
            }

            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Delete")) {
                    world.destroy_entity(entity);
                    if (m_selected_entity.index == entity.index) {
                        m_selected_entity = {hz::Entity::INVALID_INDEX, 0};
                    }
                }
                ImGui::EndPopup();
            }
        });

        ImGui::Separator();

        if (ImGui::Button("+ Add Entity", ImVec2(-1, 0))) {
            auto new_entity = world.create_entity();
            world.add_component<hz::TagComponent>(new_entity, "New Entity");
            world.add_component<hz::TransformComponent>(new_entity);
            m_selected_entity = new_entity;
        }
    }
    ImGui::End();
}

void EditorUI::draw_inspector(hz::World& world, float display_w, float display_h, float top_offset,
                              float width) {
    ImGui::SetNextWindowPos(ImVec2(display_w - width, top_offset), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(width, display_h - top_offset), ImGuiCond_Always);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;

    if (ImGui::Begin("Inspector", &m_show_inspector, flags)) {
        if (!has_selection()) {
            ImGui::TextDisabled("No entity selected");
            ImGui::End();
            return;
        }

        hz::Entity entity = m_selected_entity;

        // Tag Component
        if (auto* tag = world.get_component<hz::TagComponent>(entity)) {
            if (ImGui::CollapsingHeader("Tag", ImGuiTreeNodeFlags_DefaultOpen)) {
                char buffer[256];
                strncpy(buffer, tag->tag.c_str(), sizeof(buffer) - 1);
                buffer[sizeof(buffer) - 1] = '\0';
                if (ImGui::InputText("Name", buffer, sizeof(buffer))) {
                    tag->tag = buffer;
                }
            }
        }

        // Transform Component
        if (auto* transform = world.get_component<hz::TransformComponent>(entity)) {
            if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::DragFloat3("Position", &transform->position.x, 0.1f);
                ImGui::DragFloat3("Rotation", &transform->rotation.x, 1.0f);
                ImGui::DragFloat3("Scale", &transform->scale.x, 0.1f, 0.01f, 100.0f);
            }
        }

        // Mesh Component
        if (auto* mesh = world.get_component<hz::MeshComponent>(entity)) {
            if (ImGui::CollapsingHeader("Mesh", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Text("Mesh: %s", mesh->mesh_path.c_str());
                ImGui::Separator();
                ImGui::Text("Material");
                ImGui::ColorEdit3("Albedo", &mesh->albedo_color.x);
                ImGui::SliderFloat("Metallic", &mesh->metallic, 0.0f, 1.0f);
                ImGui::SliderFloat("Roughness", &mesh->roughness, 0.0f, 1.0f);
            }
        }

        // Light Component
        if (auto* light = world.get_component<hz::LightComponent>(entity)) {
            if (ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::ColorEdit3("Color", &light->color.x);
                ImGui::DragFloat("Intensity", &light->intensity, 0.1f, 0.0f, 100.0f);
                ImGui::DragFloat("Range", &light->range, 0.5f, 0.0f, 500.0f);
            }
        }

        ImGui::Separator();

        if (ImGui::Button("+ Add Component", ImVec2(-1, 0))) {
            ImGui::OpenPopup("AddComponentPopup");
        }

        if (ImGui::BeginPopup("AddComponentPopup")) {
            if (!world.get_component<hz::MeshComponent>(entity)) {
                if (ImGui::MenuItem("Mesh Component")) {
                    world.add_component<hz::MeshComponent>(entity);
                }
            }
            if (!world.get_component<hz::LightComponent>(entity)) {
                if (ImGui::MenuItem("Light Component")) {
                    world.add_component<hz::LightComponent>(entity);
                }
            }
            ImGui::EndPopup();
        }
    }
    ImGui::End();
}

void EditorUI::draw_stats(float fps, size_t entity_count, float display_w, float top_offset) {
    ImGui::SetNextWindowPos(ImVec2(230, top_offset + 10));
    if (ImGui::Begin("Stats", &m_show_stats, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("FPS: %.1f", fps);
        ImGui::Text("Entities: %zu", entity_count);
        ImGui::Text("Renderer: OpenGL");
    }
    ImGui::End();
}

void EditorUI::draw_scene_settings(SceneSettings& settings, float display_w, float display_h,
                                   float top_offset, float width) {
    // Position to the left of Inspector if space allows, otherwise floating
    ImGui::SetNextWindowSize(ImVec2(300, 400), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Scene Settings", &m_show_settings)) {
        if (ImGui::CollapsingHeader("Environment", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::ColorEdit3("Clear Color", &settings.clear_color.x);
            ImGui::ColorEdit3("Ambient Color", &settings.ambient_color.x);
            ImGui::DragFloat("Ambient Intensity", &settings.ambient_intensity, 0.01f, 0.0f, 5.0f);
        }

        if (ImGui::CollapsingHeader("Directional Light", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::DragFloat3("Direction", &settings.sun_direction.x, 0.01f, -1.0f, 1.0f);
            ImGui::ColorEdit3("Sun Color", &settings.sun_color.x);
            ImGui::DragFloat("Sun Intensity", &settings.sun_intensity, 0.1f, 0.0f, 20.0f);
        }

        if (ImGui::CollapsingHeader("Post Processing", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Checkbox("Bloom", &settings.bloom_enabled);
            if (settings.bloom_enabled) {
                ImGui::DragFloat("Bloom Intensity", &settings.bloom_intensity, 0.01f, 0.0f, 5.0f);
                ImGui::DragFloat("Bloom Threshold", &settings.bloom_threshold, 0.01f, 0.0f, 2.0f);
            }
            ImGui::DragFloat("Exposure", &settings.exposure, 0.1f, 0.0f, 10.0f);
        }
    }
    ImGui::End();
}

void EditorUI::draw_console(float display_h, float top_offset, float width) {
    // Bottom panel
    float console_height = 200.0f;
    ImGui::SetNextWindowPos(ImVec2(width, display_h - console_height), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(600, console_height), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Console", &m_show_console)) {
        if (ImGui::Button("Clear")) {
            m_console_logs.clear();
        }
        ImGui::Separator();

        ImGui::BeginChild("ScrollingRegion", ImVec2(0, 0), false,
                          ImGuiWindowFlags_HorizontalScrollbar);
        for (const auto& log : m_console_logs) {
            ImGui::TextUnformatted(log.c_str());
        }
        if (m_scroll_to_bottom) {
            ImGui::SetScrollHereY(1.0f);
            m_scroll_to_bottom = false;
        }
        ImGui::EndChild();
    }
    ImGui::End();
}

} // namespace game
