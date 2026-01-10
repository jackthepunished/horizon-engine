#include "imgui_layer.hpp"

#include "engine/core/log.hpp"
#include "engine/platform/window.hpp"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace hz {

ImGuiLayer::~ImGuiLayer() {
    shutdown();
}

void ImGuiLayer::init(Window& window) {
    if (m_initialized)
        return;

    m_window = window.native_handle();

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Setup style
    ImGui::StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 5.0f;
    style.FrameRounding = 3.0f;
    style.Alpha = 0.95f;

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(m_window, true);
    ImGui_ImplOpenGL3_Init("#version 410");

    m_initialized = true;
    HZ_ENGINE_INFO("ImGui initialized");
}

void ImGuiLayer::shutdown() {
    if (!m_initialized)
        return;

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    m_initialized = false;
    HZ_ENGINE_INFO("ImGui shutdown");
}

void ImGuiLayer::begin_frame() {
    if (!m_initialized)
        return;

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ImGuiLayer::end_frame() {
    if (!m_initialized)
        return;

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

} // namespace hz
