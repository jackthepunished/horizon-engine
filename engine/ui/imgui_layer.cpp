#include "engine/ui/imgui_layer.hpp"

#include <imgui.h>
// #include <backends/imgui_impl_glfw.h>
// #include <backends/imgui_impl_opengl3.h>
// #include <GLFW/glfw3.h>

namespace hz {

ImGuiLayer::~ImGuiLayer() {
    shutdown();
}

void ImGuiLayer::init(Window& window) {
    // Stub: Context creation might be needed for ImGui::Begin to work even without a backend?
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();

    // Build font atlas to avoid assertion failure
    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    m_initialized = true;
}

void ImGuiLayer::shutdown() {
    if (m_initialized) {
        // ImGui_ImplOpenGL3_Shutdown();
        // ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        m_initialized = false;
    }
}

void ImGuiLayer::begin_frame() {
    // ImGui_ImplOpenGL3_NewFrame();
    // ImGui_ImplGlfw_NewFrame();

    // Manually setup IO display size since backend is gone?
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(1920, 1080); // Placeholder
    io.DeltaTime = 1.0f / 60.0f;

    ImGui::NewFrame();
}

void ImGuiLayer::end_frame() {
    ImGui::Render();
    // ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

} // namespace hz
