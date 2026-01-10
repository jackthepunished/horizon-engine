#include "game_loop.hpp"

#include "engine/platform/platform.hpp"
#include "log.hpp"
#include "memory.hpp"

#include <algorithm>

namespace hz {

GameLoop::GameLoop(const GameLoopConfig& config) : m_config(config) {
    HZ_ENGINE_DEBUG("Game loop created: fixed timestep = {:.4f}s ({:.1f} Hz)",
                    config.fixed_timestep, 1.0 / config.fixed_timestep);
}

void GameLoop::run() {
    m_running = true;
    m_simulation_time = 0.0;
    m_total_time = 0.0;
    m_fps_timer = 0.0;
    m_frame_count = 0;

    Clock clock;
    f64 accumulator = 0.0;

    HZ_ENGINE_INFO("Game loop started");

    while (m_running) {
        // Check quit condition
        if (m_should_quit && m_should_quit()) {
            m_running = false;
            break;
        }

        // Calculate frame time
        f64 frame_time = clock.restart();
        frame_time = std::min(frame_time, m_config.max_frame_time);
        m_total_time += frame_time;

        // Input phase
        if (m_on_input) {
            m_on_input();
        }

        // Fixed timestep update phase
        accumulator += frame_time;
        m_updates_this_frame = 0;

        while (accumulator >= m_config.fixed_timestep) {
            if (m_on_update) {
                m_on_update(m_config.fixed_timestep);
            }
            m_simulation_time += m_config.fixed_timestep;
            accumulator -= m_config.fixed_timestep;
            ++m_updates_this_frame;
        }

        // Reset frame memory
        MemoryContext::reset_frame();

        // Render phase (with interpolation alpha)
        f64 alpha = accumulator / m_config.fixed_timestep;
        if (m_on_render) {
            m_on_render(alpha);
        }

        // Update FPS counter
        update_fps_counter(frame_time);
    }

    HZ_ENGINE_INFO("Game loop stopped");
}

void GameLoop::update_fps_counter(f64 frame_time) {
    ++m_frame_count;
    m_fps_timer += frame_time;

    if (m_fps_timer >= m_config.fps_log_interval) {
        m_fps = static_cast<f64>(m_frame_count) / m_fps_timer;

        if (m_config.log_fps) {
            HZ_ENGINE_DEBUG("FPS: {:.1f}", m_fps);
        }

        m_frame_count = 0;
        m_fps_timer = 0.0;
    }
}

} // namespace hz
