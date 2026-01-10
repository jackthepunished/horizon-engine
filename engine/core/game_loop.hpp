#pragma once

/**
 * @file game_loop.hpp
 * @brief Fixed timestep game loop with variable rendering
 *
 * Implements the canonical game loop pattern:
 * - Input polling
 * - Fixed timestep simulation (deterministic)
 * - Variable rendering (with interpolation alpha)
 */

#include "types.hpp"

#include <functional>

namespace hz {

// ============================================================================
// Game Loop Configuration
// ============================================================================

struct GameLoopConfig {
    f64 fixed_timestep{1.0 / 60.0}; // 60 Hz simulation
    f64 max_frame_time{0.25};       // Cap to avoid spiral of death
    bool log_fps{true};             // Log FPS periodically
    f64 fps_log_interval{5.0};      // Log every 5 seconds
};

// ============================================================================
// Game Loop Callbacks
// ============================================================================

using InputCallback = std::function<void()>;
using UpdateCallback = std::function<void(f64 dt)>;
using RenderCallback = std::function<void(f64 alpha)>;
using ShouldQuitCallback = std::function<bool()>;

// ============================================================================
// Game Loop
// ============================================================================

/**
 * @brief Fixed timestep game loop
 *
 * Ensures deterministic simulation by running updates at a fixed rate
 * while allowing variable framerate rendering with interpolation.
 */
class GameLoop {
public:
    explicit GameLoop(const GameLoopConfig& config = {});

    /**
     * @brief Run the game loop until quit
     */
    void run();

    /**
     * @brief Request the loop to stop
     */
    void quit() { m_running = false; }

    /**
     * @brief Check if the loop is running
     */
    [[nodiscard]] bool is_running() const noexcept { return m_running; }

    // ========================================================================
    // Callback Registration
    // ========================================================================

    void set_input_callback(InputCallback cb) { m_on_input = std::move(cb); }
    void set_update_callback(UpdateCallback cb) { m_on_update = std::move(cb); }
    void set_render_callback(RenderCallback cb) { m_on_render = std::move(cb); }
    void set_should_quit_callback(ShouldQuitCallback cb) { m_should_quit = std::move(cb); }

    // ========================================================================
    // Timing Info
    // ========================================================================

    /**
     * @brief Get the fixed timestep
     */
    [[nodiscard]] f64 fixed_timestep() const noexcept { return m_config.fixed_timestep; }

    /**
     * @brief Get the current simulation time
     */
    [[nodiscard]] f64 simulation_time() const noexcept { return m_simulation_time; }

    /**
     * @brief Get the total elapsed time
     */
    [[nodiscard]] f64 total_time() const noexcept { return m_total_time; }

    /**
     * @brief Get the current FPS
     */
    [[nodiscard]] f64 fps() const noexcept { return m_fps; }

    /**
     * @brief Get the number of updates per frame (for debugging)
     */
    [[nodiscard]] u32 updates_per_frame() const noexcept { return m_updates_this_frame; }

private:
    void update_fps_counter(f64 frame_time);

    GameLoopConfig m_config;
    bool m_running{false};

    InputCallback m_on_input;
    UpdateCallback m_on_update;
    RenderCallback m_on_render;
    ShouldQuitCallback m_should_quit;

    f64 m_simulation_time{0.0};
    f64 m_total_time{0.0};
    f64 m_fps{0.0};
    u32 m_updates_this_frame{0};

    // FPS tracking
    f64 m_fps_timer{0.0};
    u32 m_frame_count{0};
};

} // namespace hz
