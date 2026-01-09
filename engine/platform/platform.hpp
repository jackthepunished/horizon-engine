#pragma once

/**
 * @file platform.hpp
 * @brief Platform abstraction layer
 *
 * Provides interfaces for platform-specific functionality that can be
 * mocked for testing.
 */

#include "engine/core/types.hpp"

#include <chrono>
#include <string_view>

namespace hz {

// ============================================================================
// Time Interface
// ============================================================================

/**
 * @brief High-resolution timer for frame timing
 */
class Clock {
public:
    using TimePoint = std::chrono::steady_clock::time_point;
    using Duration = std::chrono::duration<f64>;

    Clock() : m_start(std::chrono::steady_clock::now()) {}

    /**
     * @brief Get elapsed time since clock creation in seconds
     */
    [[nodiscard]] f64 elapsed() const {
        auto now = std::chrono::steady_clock::now();
        return Duration(now - m_start).count();
    }

    /**
     * @brief Reset the clock
     */
    void reset() { m_start = std::chrono::steady_clock::now(); }

    /**
     * @brief Get elapsed time and reset
     */
    [[nodiscard]] f64 restart() {
        auto now = std::chrono::steady_clock::now();
        f64 elapsed = Duration(now - m_start).count();
        m_start = now;
        return elapsed;
    }

private:
    TimePoint m_start;
};

// ============================================================================
// Platform Info
// ============================================================================

/**
 * @brief Get the platform name
 */
[[nodiscard]] constexpr std::string_view platform_name() noexcept {
#if defined(_WIN32)
    return "Windows";
#elif defined(__APPLE__)
    return "macOS";
#elif defined(__linux__)
    return "Linux";
#else
    return "Unknown";
#endif
}

/**
 * @brief Check if running in debug mode
 */
[[nodiscard]] constexpr bool is_debug() noexcept {
#ifdef HZ_DEBUG
    return true;
#else
    return false;
#endif
}

/**
 * @brief Check if running in headless mode
 */
[[nodiscard]] constexpr bool is_headless() noexcept {
#ifdef HZ_HEADLESS
    return true;
#else
    return false;
#endif
}

} // namespace hz
