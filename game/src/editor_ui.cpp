/**
 * @file editor_ui.cpp
 * @brief Full Scene Hierarchy Editor & Property Inspector implementation
 */

#include "editor_ui.hpp"

#include <algorithm>
#include <cstring>

#include <imgui.h>

namespace game {

// ============================================================================
// Dark Theme
// ============================================================================

void EditorUI::apply_dark_theme() {
    ImGuiStyle& style = ImGui::GetStyle();

    // Rounding
    style.WindowRounding = 4.0f;
    style.ChildRounding = 4.0f;
    style.FrameRounding = 3.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabRounding = 3.0f;
    style.TabRounding = 4.0f;

    // Sizing
    style.WindowPadding = ImVec2(10, 10);
    style.FramePadding = ImVec2(6, 4);
    style.ItemSpacing = ImVec2(8, 5);
    style.ItemInnerSpacing = ImVec2(6, 4);
    style.IndentSpacing = 18.0f;
    style.ScrollbarSize = 14.0f;
    style.GrabMinSize = 10.0f;

    // Borders
    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;

    // Colors — professional dark grey with blue accent
    ImVec4* c = style.Colors;

    // Backgrounds
    c[ImGuiCol_WindowBg] = ImVec4(0.11f, 0.11f, 0.13f, 1.00f);
    c[ImGuiCol_ChildBg] = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
    c[ImGuiCol_PopupBg] = ImVec4(0.12f, 0.12f, 0.14f, 0.96f);

    // Borders
    c[ImGuiCol_Border] = ImVec4(0.22f, 0.22f, 0.26f, 1.00f);
    c[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

    // Frame (input fields, sliders etc)
    c[ImGuiCol_FrameBg] = ImVec4(0.16f, 0.16f, 0.19f, 1.00f);
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.20f, 0.20f, 0.24f, 1.00f);
    c[ImGuiCol_FrameBgActive] = ImVec4(0.24f, 0.24f, 0.29f, 1.00f);

    // Title bar
    c[ImGuiCol_TitleBg] = ImVec4(0.08f, 0.08f, 0.10f, 1.00f);
    c[ImGuiCol_TitleBgActive] = ImVec4(0.10f, 0.10f, 0.13f, 1.00f);
    c[ImGuiCol_TitleBgCollapsed] = ImVec4(0.08f, 0.08f, 0.10f, 0.50f);

    // Menu bar
    c[ImGuiCol_MenuBarBg] = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);

    // Scrollbar
    c[ImGuiCol_ScrollbarBg] = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
    c[ImGuiCol_ScrollbarGrab] = ImVec4(0.25f, 0.25f, 0.30f, 1.00f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.30f, 0.30f, 0.36f, 1.00f);
    c[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.35f, 0.35f, 0.42f, 1.00f);

    // Buttons (accent blue)
    c[ImGuiCol_Button] = ImVec4(0.22f, 0.35f, 0.55f, 1.00f);
    c[ImGuiCol_ButtonHovered] = ImVec4(0.26f, 0.42f, 0.65f, 1.00f);
    c[ImGuiCol_ButtonActive] = ImVec4(0.18f, 0.30f, 0.48f, 1.00f);

    // Header (collapsing headers, tree nodes)
    c[ImGuiCol_Header] = ImVec4(0.18f, 0.18f, 0.22f, 1.00f);
    c[ImGuiCol_HeaderHovered] = ImVec4(0.22f, 0.35f, 0.55f, 0.80f);
    c[ImGuiCol_HeaderActive] = ImVec4(0.22f, 0.35f, 0.55f, 1.00f);

    // Separator
    c[ImGuiCol_Separator] = ImVec4(0.22f, 0.22f, 0.26f, 1.00f);
    c[ImGuiCol_SeparatorHovered] = ImVec4(0.30f, 0.45f, 0.65f, 0.78f);
    c[ImGuiCol_SeparatorActive] = ImVec4(0.30f, 0.45f, 0.65f, 1.00f);

    // Resize grip
    c[ImGuiCol_ResizeGrip] = ImVec4(0.22f, 0.35f, 0.55f, 0.25f);
    c[ImGuiCol_ResizeGripHovered] = ImVec4(0.22f, 0.35f, 0.55f, 0.67f);
    c[ImGuiCol_ResizeGripActive] = ImVec4(0.22f, 0.35f, 0.55f, 0.95f);

    // Tabs
    c[ImGuiCol_Tab] = ImVec4(0.14f, 0.14f, 0.17f, 1.00f);
    c[ImGuiCol_TabHovered] = ImVec4(0.22f, 0.35f, 0.55f, 0.80f);
    c[ImGuiCol_TabActive] = ImVec4(0.22f, 0.35f, 0.55f, 1.00f);
    c[ImGuiCol_TabUnfocused] = ImVec4(0.12f, 0.12f, 0.15f, 1.00f);
    c[ImGuiCol_TabUnfocusedActive] = ImVec4(0.18f, 0.18f, 0.22f, 1.00f);

    // Checkboxes / slider grab
    c[ImGuiCol_CheckMark] = ImVec4(0.40f, 0.65f, 1.00f, 1.00f);
    c[ImGuiCol_SliderGrab] = ImVec4(0.30f, 0.50f, 0.78f, 1.00f);
    c[ImGuiCol_SliderGrabActive] = ImVec4(0.40f, 0.60f, 0.88f, 1.00f);

    // Text
    c[ImGuiCol_Text] = ImVec4(0.90f, 0.90f, 0.92f, 1.00f);
    c[ImGuiCol_TextDisabled] = ImVec4(0.45f, 0.45f, 0.50f, 1.00f);
    c[ImGuiCol_TextSelectedBg] = ImVec4(0.22f, 0.35f, 0.55f, 0.43f);

    // Drag/Drop
    c[ImGuiCol_DragDropTarget] = ImVec4(0.40f, 0.65f, 1.00f, 0.90f);
}

// ============================================================================
// Main Draw
// ============================================================================

void EditorUI::draw(hz::Scene& scene, SceneSettings& settings, float fps, size_t entity_count,
                    const hz::RenderStats& render_stats) {
    ImGuiIO& io = ImGui::GetIO();
    float display_w = io.DisplaySize.x;
    float display_h = io.DisplaySize.y;

    const float hierarchy_width = 240.0f;
    const float inspector_width = 320.0f;
    const float menu_bar_height = 20.0f;

    draw_menu_bar();

    if (m_show_toolbar) {
        draw_toolbar(display_w, menu_bar_height);
    }

    float top_offset = menu_bar_height + (m_show_toolbar ? 40.0f : 0.0f);

    if (m_show_hierarchy) {
        draw_hierarchy(scene, display_h, top_offset, hierarchy_width);
    }

    if (m_show_inspector) {
        draw_inspector(scene, display_w, display_h, top_offset, inspector_width);
    }

    if (m_show_settings) {
        draw_scene_settings(settings, display_w, display_h, top_offset, inspector_width);
    }

    if (m_show_stats) {
        draw_stats(fps, entity_count, render_stats, display_w, top_offset);
    }

    if (m_show_console) {
        draw_console(display_h, top_offset, hierarchy_width);
    }
}

// ============================================================================
// Menu Bar
// ============================================================================

void EditorUI::draw_menu_bar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Save Scene", "Ctrl+S")) { /* TODO */
            }
            if (ImGui::MenuItem("Load Scene", "Ctrl+O")) { /* TODO */
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "Esc")) { /* TODO */
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Undo", "Ctrl+Z", false, false)) { /* TODO */
            }
            if (ImGui::MenuItem("Redo", "Ctrl+Y", false, false)) { /* TODO */
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Hierarchy", "H", &m_show_hierarchy);
            ImGui::MenuItem("Inspector", "I", &m_show_inspector);
            ImGui::MenuItem("Scene Settings", nullptr, &m_show_settings);
            ImGui::MenuItem("Stats", nullptr, &m_show_stats);
            ImGui::MenuItem("Console", "`", &m_show_console);
            ImGui::MenuItem("Toolbar", nullptr, &m_show_toolbar);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Create")) {
            if (ImGui::MenuItem("Empty Entity")) {
                m_req_add_empty = true;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Cube")) {
                m_req_add_cube = true;
            }
            if (ImGui::MenuItem("Sphere")) {
                m_req_add_sphere = true;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Point Light")) {
                m_req_add_light = true;
            }
            if (ImGui::MenuItem("Camera")) {
                m_req_add_camera = true;
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("Controls")) { /* TODO */
            }
            if (ImGui::MenuItem("About")) { /* TODO */
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
}

// ============================================================================
// Toolbar
// ============================================================================

void EditorUI::draw_toolbar(float display_w, float menu_bar_height) {
    ImGui::SetNextWindowPos(ImVec2(0, menu_bar_height));
    ImGui::SetNextWindowSize(ImVec2(display_w, 40.0f));
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoScrollWithMouse |
                             ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.09f, 0.09f, 0.11f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));

    if (ImGui::Begin("##Toolbar", nullptr, flags)) {
        // File operations
        if (ImGui::Button("Save")) { /* TODO */
        }
        ImGui::SameLine();
        if (ImGui::Button("Load")) { /* TODO */
        }

        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();

        // Entity creation
        if (ImGui::Button("+ Cube")) {
            m_req_add_cube = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("+ Sphere")) {
            m_req_add_sphere = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("+ Light")) {
            m_req_add_light = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("+ Camera")) {
            m_req_add_camera = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("+ Empty")) {
            m_req_add_empty = true;
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

// ============================================================================
// Entity Icon Helper
// ============================================================================

const char* EditorUI::entity_icon(const entt::registry& reg, hz::Entity entity) {
    if (reg.any_of<hz::CameraComponent>(entity))
        return "[C] ";
    if (reg.any_of<hz::LightComponent>(entity))
        return "[L] ";
    if (reg.any_of<hz::MeshComponent>(entity))
        return "[M] ";
    return "    ";
}

// ============================================================================
// Hierarchy Panel
// ============================================================================

void EditorUI::draw_hierarchy(hz::Scene& scene, float display_h, float top_offset, float width) {
    ImGui::SetNextWindowPos(ImVec2(0, top_offset), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(width, display_h - top_offset), ImGuiCond_Always);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;

    if (ImGui::Begin("Hierarchy", &m_show_hierarchy, flags)) {
        // --- Search bar ---
        ImGui::PushItemWidth(-1);
        ImGui::InputTextWithHint("##Search", "Search entities...", m_search_buffer,
                                 sizeof(m_search_buffer));
        ImGui::PopItemWidth();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // --- Entity list ---
        std::string search_lower;
        if (m_search_buffer[0] != '\0') {
            search_lower = m_search_buffer;
            std::transform(search_lower.begin(), search_lower.end(), search_lower.begin(),
                           [](char c) { return static_cast<char>(std::tolower(c)); });
        }

        bool any_visible = false;

        // Iterate all entities using the same pattern as the serializer
        auto entity_view = scene.registry().view<hz::Entity>();
        entity_view.each([&](auto entity) {
            if (!scene.registry().valid(entity))
                return; // continue in lambda

            // Get display name
            std::string name;
            if (auto* tag = scene.registry().try_get<hz::TagComponent>(entity)) {
                name = tag->tag;
            } else {
                name = "Entity " + std::to_string(static_cast<uint32_t>(entity));
            }

            // Filter by search
            if (!search_lower.empty()) {
                std::string name_lower = name;
                std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(),
                               [](char c) { return static_cast<char>(std::tolower(c)); });
                if (name_lower.find(search_lower) == std::string::npos)
                    return; // continue in lambda
            }

            any_visible = true;

            // Icon prefix
            const char* icon = entity_icon(scene.registry(), entity);

            // Build display string with icon
            std::string display = std::string(icon) + name;

            // Tree node flags
            ImGuiTreeNodeFlags node_flags = ImGuiTreeNodeFlags_Leaf |
                                            ImGuiTreeNodeFlags_NoTreePushOnOpen |
                                            ImGuiTreeNodeFlags_SpanAvailWidth;
            if (m_selected_entity == entity) {
                node_flags |= ImGuiTreeNodeFlags_Selected;
            }

            ImGui::PushID(static_cast<int>(static_cast<uint32_t>(entity)));
            ImGui::TreeNodeEx(display.c_str(), node_flags);

            // Selection
            if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
                m_selected_entity = entity;
            }

            // Context menu
            if (ImGui::BeginPopupContextItem("EntityContextMenu")) {
                if (ImGui::MenuItem("Duplicate")) {
                    // TODO: implement entity duplication
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Delete", "Del")) {
                    m_req_delete = true;
                    m_req_delete_target = entity;
                    if (m_selected_entity == entity) {
                        m_selected_entity = entt::null;
                    }
                }
                ImGui::EndPopup();
            }

            ImGui::PopID();
        });

        if (!any_visible && m_search_buffer[0] != '\0') {
            ImGui::TextDisabled("No matching entities");
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // --- Create entity dropdown ---
        if (ImGui::Button("+ Add Entity", ImVec2(-1, 0))) {
            ImGui::OpenPopup("AddEntityPopup");
        }

        if (ImGui::BeginPopup("AddEntityPopup")) {
            if (ImGui::MenuItem("Empty Entity")) {
                m_req_add_empty = true;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Cube")) {
                m_req_add_cube = true;
            }
            if (ImGui::MenuItem("Sphere")) {
                m_req_add_sphere = true;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Point Light")) {
                m_req_add_light = true;
            }
            if (ImGui::MenuItem("Camera")) {
                m_req_add_camera = true;
            }
            ImGui::EndPopup();
        }
    }
    ImGui::End();
}

// ============================================================================
// Inspector Panel
// ============================================================================

void EditorUI::draw_inspector(hz::Scene& scene, float display_w, float display_h, float top_offset,
                              float width) {
    ImGui::SetNextWindowPos(ImVec2(display_w - width, top_offset), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(width, display_h - top_offset), ImGuiCond_Always);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;

    if (ImGui::Begin("Inspector", &m_show_inspector, flags)) {
        if (!has_selection()) {
            ImGui::TextDisabled("No entity selected");
            ImGui::TextDisabled("Select an entity in the Hierarchy panel");
            ImGui::End();
            return;
        }

        hz::Entity entity = m_selected_entity;
        if (!scene.registry().valid(entity)) {
            m_selected_entity = entt::null;
            ImGui::TextDisabled("Selected entity no longer exists");
            ImGui::End();
            return;
        }

        // Entity header with ID
        ImGui::Text("Entity #%u", static_cast<uint32_t>(entity));
        ImGui::Separator();
        ImGui::Spacing();

        // ------ TagComponent ------
        if (auto* tag = scene.registry().try_get<hz::TagComponent>(entity)) {
            draw_tag_section(*tag);
        }

        // ------ TransformComponent ------
        if (auto* transform = scene.registry().try_get<hz::TransformComponent>(entity)) {
            bool remove = false;
            if (begin_component_section("Transform", "##TransformSection", false, remove)) {
                draw_transform_section(*transform);
            }
        }

        // ------ MeshComponent ------
        if (auto* mesh = scene.registry().try_get<hz::MeshComponent>(entity)) {
            bool remove = false;
            if (begin_component_section("Mesh", "##MeshSection", true, remove)) {
                draw_mesh_section(*mesh);
            }
            if (remove)
                scene.registry().remove<hz::MeshComponent>(entity);
        }

        // ------ LightComponent ------
        if (auto* light = scene.registry().try_get<hz::LightComponent>(entity)) {
            bool remove = false;
            if (begin_component_section("Light", "##LightSection", true, remove)) {
                draw_light_section(*light);
            }
            if (remove)
                scene.registry().remove<hz::LightComponent>(entity);
        }

        // ------ CameraComponent ------
        if (auto* cam = scene.registry().try_get<hz::CameraComponent>(entity)) {
            bool remove = false;
            if (begin_component_section("Camera", "##CameraSection", true, remove)) {
                draw_camera_section(*cam);
            }
            if (remove)
                scene.registry().remove<hz::CameraComponent>(entity);
        }

        // ------ RigidBodyComponent ------
        if (auto* rb = scene.registry().try_get<hz::RigidBodyComponent>(entity)) {
            bool remove = false;
            if (begin_component_section("Rigid Body", "##RBSection", true, remove)) {
                draw_rigidbody_section(*rb);
            }
            if (remove)
                scene.registry().remove<hz::RigidBodyComponent>(entity);
        }

        // ------ BoxColliderComponent ------
        if (auto* bc = scene.registry().try_get<hz::BoxColliderComponent>(entity)) {
            bool remove = false;
            if (begin_component_section("Box Collider", "##BoxColSection", true, remove)) {
                draw_box_collider_section(*bc);
            }
            if (remove)
                scene.registry().remove<hz::BoxColliderComponent>(entity);
        }

        // ------ SphereColliderComponent ------
        if (auto* sc = scene.registry().try_get<hz::SphereColliderComponent>(entity)) {
            bool remove = false;
            if (begin_component_section("Sphere Collider", "##SphereColSection", true, remove)) {
                draw_sphere_collider_section(*sc);
            }
            if (remove)
                scene.registry().remove<hz::SphereColliderComponent>(entity);
        }

        // ------ CapsuleColliderComponent ------
        if (auto* cc = scene.registry().try_get<hz::CapsuleColliderComponent>(entity)) {
            bool remove = false;
            if (begin_component_section("Capsule Collider", "##CapsuleColSection", true, remove)) {
                draw_capsule_collider_section(*cc);
            }
            if (remove)
                scene.registry().remove<hz::CapsuleColliderComponent>(entity);
        }

        // ------ IKTargetComponent ------
        if (auto* ik = scene.registry().try_get<hz::IKTargetComponent>(entity)) {
            bool remove = false;
            if (begin_component_section("IK Target", "##IKSection", true, remove)) {
                draw_ik_target_section(*ik);
            }
            if (remove)
                scene.registry().remove<hz::IKTargetComponent>(entity);
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // --- Add Component button ---
        draw_add_component_popup(scene, entity);
    }
    ImGui::End();
}

// ============================================================================
// Component Section Helpers
// ============================================================================

bool EditorUI::begin_component_section(const char* label, const char* id, bool can_remove,
                                       bool& out_remove_requested) {
    out_remove_requested = false;

    ImGui::PushID(id);

    // Collapsing header with settings gear via context menu
    bool open = ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_DefaultOpen);

    // Right-click context menu for removal
    if (can_remove && ImGui::BeginPopupContextItem("ComponentContextMenu")) {
        if (ImGui::MenuItem("Remove Component")) {
            out_remove_requested = true;
        }
        ImGui::EndPopup();
    }

    ImGui::PopID();
    return open;
}

// ============================================================================
// Tag Section
// ============================================================================

void EditorUI::draw_tag_section(hz::TagComponent& tag) {
    char buffer[256];
    std::strncpy(buffer, tag.tag.c_str(), sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    ImGui::PushItemWidth(-1);
    if (ImGui::InputText("##EntityName", buffer, sizeof(buffer),
                         ImGuiInputTextFlags_EnterReturnsTrue)) {
        tag.tag = buffer;
    }
    // Also update on deactivation (user clicked away)
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        tag.tag = buffer;
    }
    ImGui::PopItemWidth();

    ImGui::Spacing();
}

// ============================================================================
// Transform Section
// ============================================================================

void EditorUI::draw_transform_section(hz::TransformComponent& transform) {
    // Position
    ImGui::Text("Position");
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 20.0f);
    if (ImGui::SmallButton("R##pos")) {
        transform.position = glm::vec3(0.0f);
    }
    ImGui::DragFloat3("##Position", &transform.position.x, 0.1f);

    // Rotation
    ImGui::Text("Rotation");
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 20.0f);
    if (ImGui::SmallButton("R##rot")) {
        transform.rotation = glm::vec3(0.0f);
    }
    ImGui::DragFloat3("##Rotation", &transform.rotation.x, 1.0f, -360.0f, 360.0f);

    // Scale
    ImGui::Text("Scale");
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 20.0f);
    if (ImGui::SmallButton("R##scl")) {
        transform.scale = glm::vec3(1.0f);
    }
    ImGui::DragFloat3("##Scale", &transform.scale.x, 0.05f, 0.001f, 100.0f);
}

// ============================================================================
// Mesh Section
// ============================================================================

void EditorUI::draw_mesh_section(hz::MeshComponent& mesh) {
    // Mesh type selector
    const char* mesh_types[] = {"Primitive", "Model"};
    int current_type = static_cast<int>(mesh.mesh_type);
    if (ImGui::Combo("Type", &current_type, mesh_types, 2)) {
        mesh.mesh_type = static_cast<hz::MeshComponent::MeshType>(current_type);
    }

    if (mesh.mesh_type == hz::MeshComponent::MeshType::Primitive) {
        // Primitive selector
        const char* primitives[] = {"cube", "sphere", "plane"};
        int current_prim = 0;
        if (mesh.primitive_name == "sphere")
            current_prim = 1;
        else if (mesh.primitive_name == "plane")
            current_prim = 2;

        if (ImGui::Combo("Primitive", &current_prim, primitives, 3)) {
            mesh.primitive_name = primitives[current_prim];
            mesh.mesh_path = mesh.primitive_name;
        }
    } else {
        ImGui::Text("Model Handle: %u (gen %u)", mesh.model.index, mesh.model.generation);
    }

    ImGui::Checkbox("Cast Shadows", &mesh.cast_shadows);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Material");

    ImGui::ColorEdit3("Albedo", &mesh.albedo_color.x);
    ImGui::SliderFloat("Metallic", &mesh.metallic, 0.0f, 1.0f);
    ImGui::SliderFloat("Roughness", &mesh.roughness, 0.0f, 1.0f);

    // Show texture paths if set
    if (!mesh.albedo_path.empty()) {
        ImGui::TextDisabled("Albedo Tex: %s", mesh.albedo_path.c_str());
    }
    if (!mesh.normal_path.empty()) {
        ImGui::TextDisabled("Normal Tex: %s", mesh.normal_path.c_str());
    }
}

// ============================================================================
// Light Section
// ============================================================================

void EditorUI::draw_light_section(hz::LightComponent& light) {
    const char* light_types[] = {"Directional", "Point"};
    int current_type = static_cast<int>(light.type);
    if (ImGui::Combo("Type", &current_type, light_types, 2)) {
        light.type = static_cast<hz::LightType>(current_type);
    }

    ImGui::ColorEdit3("Color", &light.color.x);
    ImGui::DragFloat("Intensity", &light.intensity, 0.1f, 0.0f, 100.0f);

    if (light.type == hz::LightType::Point) {
        ImGui::DragFloat("Range", &light.range, 0.5f, 0.0f, 500.0f);
    }
}

// ============================================================================
// Camera Section
// ============================================================================

void EditorUI::draw_camera_section(hz::CameraComponent& cam) {
    ImGui::SliderFloat("FOV", &cam.fov, 10.0f, 120.0f, "%.0f deg");
    ImGui::DragFloat("Near Plane", &cam.near_plane, 0.01f, 0.001f, 10.0f, "%.3f");
    ImGui::DragFloat("Far Plane", &cam.far_plane, 10.0f, 10.0f, 10000.0f, "%.0f");
    ImGui::Checkbox("Primary", &cam.primary);
}

// ============================================================================
// Rigid Body Section
// ============================================================================

void EditorUI::draw_rigidbody_section(hz::RigidBodyComponent& rb) {
    const char* body_types[] = {"Static", "Dynamic", "Kinematic"};
    int current_type = static_cast<int>(rb.type);
    if (ImGui::Combo("Body Type", &current_type, body_types, 3)) {
        rb.type = static_cast<hz::RigidBodyComponent::BodyType>(current_type);
    }

    if (rb.type == hz::RigidBodyComponent::BodyType::Dynamic) {
        ImGui::DragFloat("Mass", &rb.mass, 0.1f, 0.001f, 10000.0f, "%.2f kg");
    }

    ImGui::Checkbox("Fixed Rotation", &rb.fixed_rotation);

    // Runtime info
    ImGui::TextDisabled("Physics body %s", rb.created ? "created" : "pending");
}

// ============================================================================
// Box Collider Section
// ============================================================================

void EditorUI::draw_box_collider_section(hz::BoxColliderComponent& bc) {
    ImGui::DragFloat3("Half Extents", &bc.half_extents.x, 0.05f, 0.001f, 100.0f);
    ImGui::DragFloat3("Offset", &bc.offset.x, 0.05f);
}

// ============================================================================
// Sphere Collider Section
// ============================================================================

void EditorUI::draw_sphere_collider_section(hz::SphereColliderComponent& sc) {
    ImGui::DragFloat("Radius", &sc.radius, 0.05f, 0.001f, 100.0f);
    ImGui::DragFloat3("Offset", &sc.offset.x, 0.05f);
}

// ============================================================================
// Capsule Collider Section
// ============================================================================

void EditorUI::draw_capsule_collider_section(hz::CapsuleColliderComponent& cc) {
    ImGui::DragFloat("Radius", &cc.radius, 0.05f, 0.001f, 100.0f);
    ImGui::DragFloat("Half Height", &cc.half_height, 0.05f, 0.001f, 100.0f);
    ImGui::DragFloat3("Offset", &cc.offset.x, 0.05f);

    // Visual info
    float total_height = 2.0f * cc.half_height + 2.0f * cc.radius;
    ImGui::TextDisabled("Total height: %.2f", static_cast<double>(total_height));
}

// ============================================================================
// IK Target Section
// ============================================================================

void EditorUI::draw_ik_target_section(hz::IKTargetComponent& ik) {
    ImGui::Checkbox("Enabled", &ik.enabled);

    ImGui::DragInt("Root Bone", &ik.root_bone_id, 1, -1, 255);
    ImGui::DragInt("Mid Bone", &ik.mid_bone_id, 1, -1, 255);
    ImGui::DragInt("End Bone", &ik.end_bone_id, 1, -1, 255);

    ImGui::DragFloat3("Target Pos", &ik.target_position.x, 0.1f);
    ImGui::DragFloat3("Pole Vector", &ik.pole_vector.x, 0.05f);

    ImGui::SliderFloat("Weight", &ik.weight, 0.0f, 1.0f);
}

// ============================================================================
// Add Component Popup
// ============================================================================

void EditorUI::draw_add_component_popup(hz::Scene& scene, hz::Entity entity) {
    float button_width = ImGui::GetContentRegionAvail().x;
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.32f, 0.50f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.22f, 0.40f, 0.62f, 1.0f));

    if (ImGui::Button("+ Add Component", ImVec2(button_width, 28))) {
        ImGui::OpenPopup("AddComponentPopup");
    }

    ImGui::PopStyleColor(2);

    if (ImGui::BeginPopup("AddComponentPopup")) {
        ImGui::TextDisabled("--- Rendering ---");

        if (!scene.registry().any_of<hz::MeshComponent>(entity)) {
            if (ImGui::MenuItem("Mesh")) {
                scene.registry().emplace<hz::MeshComponent>(entity);
            }
        }
        if (!scene.registry().any_of<hz::LightComponent>(entity)) {
            if (ImGui::MenuItem("Light")) {
                scene.registry().emplace<hz::LightComponent>(entity);
            }
        }
        if (!scene.registry().any_of<hz::CameraComponent>(entity)) {
            if (ImGui::MenuItem("Camera")) {
                scene.registry().emplace<hz::CameraComponent>(entity);
            }
        }

        ImGui::Separator();
        ImGui::TextDisabled("--- Physics ---");

        if (!scene.registry().any_of<hz::RigidBodyComponent>(entity)) {
            if (ImGui::MenuItem("Rigid Body")) {
                scene.registry().emplace<hz::RigidBodyComponent>(entity);
            }
        }
        if (!scene.registry().any_of<hz::BoxColliderComponent>(entity)) {
            if (ImGui::MenuItem("Box Collider")) {
                scene.registry().emplace<hz::BoxColliderComponent>(entity);
            }
        }
        if (!scene.registry().any_of<hz::SphereColliderComponent>(entity)) {
            if (ImGui::MenuItem("Sphere Collider")) {
                scene.registry().emplace<hz::SphereColliderComponent>(entity);
            }
        }
        if (!scene.registry().any_of<hz::CapsuleColliderComponent>(entity)) {
            if (ImGui::MenuItem("Capsule Collider")) {
                scene.registry().emplace<hz::CapsuleColliderComponent>(entity);
            }
        }

        ImGui::Separator();
        ImGui::TextDisabled("--- Animation ---");

        if (!scene.registry().any_of<hz::IKTargetComponent>(entity)) {
            if (ImGui::MenuItem("IK Target")) {
                scene.registry().emplace<hz::IKTargetComponent>(entity);
            }
        }

        ImGui::EndPopup();
    }
}

// ============================================================================
// Scene Settings Panel
// ============================================================================

void EditorUI::draw_scene_settings(SceneSettings& settings, [[maybe_unused]] float display_w,
                                   [[maybe_unused]] float display_h,
                                   [[maybe_unused]] float top_offset,
                                   [[maybe_unused]] float width) {
    ImGui::SetNextWindowSize(ImVec2(320, 440), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Scene Settings", &m_show_settings)) {
        // Environment
        if (ImGui::CollapsingHeader("Environment", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::ColorEdit3("Clear Color", &settings.clear_color.x);
            ImGui::ColorEdit3("Ambient Color", &settings.ambient_color.x);
            ImGui::DragFloat("Ambient Intensity", &settings.ambient_intensity, 0.01f, 0.0f, 5.0f);

            ImGui::Spacing();
            ImGui::Checkbox("Fog Enabled", &settings.fog_enabled);
            if (settings.fog_enabled) {
                ImGui::DragFloat("Fog Density", &settings.fog_density, 0.001f, 0.0f, 0.1f);
                ImGui::DragFloat("Fog Gradient", &settings.fog_gradient, 0.05f, 0.1f, 10.0f);
            }
        }

        // Sun
        if (ImGui::CollapsingHeader("Directional Light", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::DragFloat3("Direction", &settings.sun_direction.x, 0.01f, -1.0f, 1.0f);
            ImGui::ColorEdit3("Sun Color", &settings.sun_color.x);
            ImGui::DragFloat("Sun Intensity", &settings.sun_intensity, 0.1f, 0.0f, 20.0f);
        }

        // Post-processing
        if (ImGui::CollapsingHeader("Post Processing", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::DragFloat("Exposure", &settings.exposure, 0.05f, 0.0f, 10.0f);

            ImGui::Spacing();
            ImGui::Checkbox("Bloom", &settings.bloom_enabled);
            if (settings.bloom_enabled) {
                ImGui::DragFloat("Bloom Intensity", &settings.bloom_intensity, 0.01f, 0.0f, 5.0f);
                ImGui::DragFloat("Bloom Threshold", &settings.bloom_threshold, 0.01f, 0.0f, 2.0f);
            }
        }
    }
    ImGui::End();
}

// ============================================================================
// Stats Panel
// ============================================================================

void EditorUI::draw_stats(float fps, size_t entity_count, const hz::RenderStats& stats,
                          [[maybe_unused]] float display_w, float top_offset) {
    ImGui::SetNextWindowPos(ImVec2(250, top_offset + 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(0.85f);

    if (ImGui::Begin("Stats", &m_show_stats,
                     ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing)) {
        // General
        ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "Performance");
        ImGui::Text("FPS:        %.1f", static_cast<double>(fps));
        ImGui::Text("Frame:      %.2f ms", static_cast<double>(stats.total_frame_ms));

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Renderer
        ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "Renderer");
        ImGui::Text("Backend:    Vulkan 1.3");
        ImGui::Text("ECS:        EnTT");
        ImGui::Text("Entities:   %zu", entity_count);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Draw stats
        ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "Draw Stats");
        ImGui::Text("Draw Calls: %u", stats.draw_calls);
        ImGui::Text("Triangles:  %u", stats.triangles);
        ImGui::Text("Visible:    %u", stats.visible_objects);
        ImGui::Text("Culled:     %u", stats.culled_objects);
        ImGui::Text("Lights:     %u", stats.active_lights);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Pass timings
        ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "Pass Timings");
        ImGui::Text("Geometry:   %.2f ms", static_cast<double>(stats.geometry_pass_ms));
        ImGui::Text("Shadows:    %.2f ms", static_cast<double>(stats.shadow_pass_ms));
        ImGui::Text("Lighting:   %.2f ms", static_cast<double>(stats.lighting_pass_ms));
        ImGui::Text("PostFX:     %.2f ms", static_cast<double>(stats.post_process_ms));
    }
    ImGui::End();
}

// ============================================================================
// Console Panel
// ============================================================================

void EditorUI::draw_console(float display_h, [[maybe_unused]] float top_offset, float width) {
    float console_height = 200.0f;
    ImGui::SetNextWindowPos(ImVec2(width, display_h - console_height), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(600, console_height), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Console", &m_show_console)) {
        // Toolbar
        if (ImGui::Button("Clear")) {
            m_console_logs.clear();
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(%zu messages)", m_console_logs.size());

        ImGui::Separator();

        // Log area
        ImGui::BeginChild("ScrollingRegion", ImVec2(0, 0), false,
                          ImGuiWindowFlags_HorizontalScrollbar);

        for (const auto& log : m_console_logs) {
            // Color code by prefix
            if (log.find("[ERR") != std::string::npos || log.find("[FATAL") != std::string::npos) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
                ImGui::TextUnformatted(log.c_str());
                ImGui::PopStyleColor();
            } else if (log.find("[WARN") != std::string::npos) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.85f, 0.3f, 1.0f));
                ImGui::TextUnformatted(log.c_str());
                ImGui::PopStyleColor();
            } else {
                ImGui::TextUnformatted(log.c_str());
            }
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
