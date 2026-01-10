#pragma once

/**
 * @file debug_overlay.hpp
 * @brief Debug overlay with FPS counter and stats
 */

#include "engine/core/types.hpp"

namespace hz {

/**
 * @brief Debug overlay showing performance stats
 */
class DebugOverlay {
public:
    DebugOverlay() = default;
    ~DebugOverlay() = default;

    /**
     * @brief Draw the debug overlay
     * @param fps Current frames per second
     * @param frame_time Frame time in milliseconds
     * @param physics_bodies Number of physics bodies
     */
    void draw(f32 fps, f32 frame_time, u32 physics_bodies = 0);

    /**
     * @brief Toggle overlay visibility
     */
    void toggle() { m_visible = !m_visible; }

    /**
     * @brief Check if visible
     */
    [[nodiscard]] bool is_visible() const { return m_visible; }

    /**
     * @brief Set visibility
     */
    void set_visible(bool visible) { m_visible = visible; }

private:
    bool m_visible{true};

    // Frame time history for graph
    static constexpr size_t HISTORY_SIZE = 120;
    f32 m_frame_times[HISTORY_SIZE]{};
    size_t m_frame_index{0};
};

} // namespace hz
