#include "debug_overlay.hpp"

#include <imgui.h>

namespace hz {

void DebugOverlay::draw(f32 fps, f32 frame_time, u32 physics_bodies) {
    if (!m_visible)
        return;

    // Store frame time in history
    m_frame_times[m_frame_index] = frame_time;
    m_frame_index = (m_frame_index + 1) % HISTORY_SIZE;

    // Set window position to top-left corner
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(0.75f);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                             ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
                             ImGuiWindowFlags_NoMove;

    if (ImGui::Begin("##DebugOverlay", nullptr, flags)) {
        // FPS counter
        ImGui::Text("FPS: %.1f", static_cast<double>(fps));
        ImGui::Text("Frame: %.2f ms", static_cast<double>(frame_time));

        ImGui::Separator();

        // Frame time graph
        ImGui::PlotLines("##FrameGraph", m_frame_times, HISTORY_SIZE,
                         static_cast<int>(m_frame_index), nullptr, 0.0f, 33.3f, ImVec2(150, 40));

        if (physics_bodies > 0) {
            ImGui::Separator();
            ImGui::Text("Physics: %u bodies", physics_bodies);
        }

        ImGui::Separator();
        ImGui::TextDisabled("F3 to toggle");
    }
    ImGui::End();
}

} // namespace hz
