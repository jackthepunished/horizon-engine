#pragma once

/**
 * @file log.hpp
 * @brief Logging system for the Horizon Engine
 *
 * Provides compile-time filtered logging with multiple severity levels.
 * Wraps spdlog for high-performance, thread-safe logging.
 */

#include "types.hpp"

#include <memory>

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace hz {

/**
 * @brief Logging severity levels
 */
enum class LogLevel : u8 {
    Trace = 0,
    Debug = 1,
    Info = 2,
    Warn = 3,
    Error = 4,
    Fatal = 5,
    Off = 6
};

/**
 * @brief Central logging system
 *
 * Manages engine and application loggers with configurable output
 * and severity filtering.
 */
class Log {
public:
    /**
     * @brief Initialize the logging system
     * @param engine_level Minimum level for engine logs
     * @param app_level Minimum level for application logs
     */
    static void init(LogLevel engine_level = LogLevel::Trace, LogLevel app_level = LogLevel::Trace);

    /**
     * @brief Shutdown the logging system
     */
    static void shutdown();

    /**
     * @brief Get the engine logger
     */
    [[nodiscard]] static std::shared_ptr<spdlog::logger>& engine_logger() {
        return s_engine_logger;
    }

    /**
     * @brief Get the application logger
     */
    [[nodiscard]] static std::shared_ptr<spdlog::logger>& app_logger() { return s_app_logger; }

private:
    static std::shared_ptr<spdlog::logger> s_engine_logger;
    static std::shared_ptr<spdlog::logger> s_app_logger;
};

} // namespace hz

// ============================================================================
// Logging Macros
// ============================================================================

// Engine logging (internal use)
#define HZ_ENGINE_TRACE(...) ::hz::Log::engine_logger()->trace(__VA_ARGS__)
#define HZ_ENGINE_DEBUG(...) ::hz::Log::engine_logger()->debug(__VA_ARGS__)
#define HZ_ENGINE_INFO(...) ::hz::Log::engine_logger()->info(__VA_ARGS__)
#define HZ_ENGINE_WARN(...) ::hz::Log::engine_logger()->warn(__VA_ARGS__)
#define HZ_ENGINE_ERROR(...) ::hz::Log::engine_logger()->error(__VA_ARGS__)
#define HZ_ENGINE_FATAL(...) ::hz::Log::engine_logger()->critical(__VA_ARGS__)

// Application logging (game code use)
#define HZ_LOG_TRACE(...) ::hz::Log::app_logger()->trace(__VA_ARGS__)
#define HZ_LOG_DEBUG(...) ::hz::Log::app_logger()->debug(__VA_ARGS__)
#define HZ_LOG_INFO(...) ::hz::Log::app_logger()->info(__VA_ARGS__)
#define HZ_LOG_WARN(...) ::hz::Log::app_logger()->warn(__VA_ARGS__)
#define HZ_LOG_ERROR(...) ::hz::Log::app_logger()->error(__VA_ARGS__)
#define HZ_LOG_CRITICAL(...) ::hz::Log::app_logger()->critical(__VA_ARGS__)

// Deprecated macros (use HZ_LOG_* instead)
#define HZ_ERROR(...) ::hz::Log::app_logger()->error(__VA_ARGS__)
#define HZ_FATAL(...) ::hz::Log::app_logger()->critical(__VA_ARGS__)

// Utility macros
#define HZ_UNUSED(x) (void)(x)

// ============================================================================
// Assertions
// ============================================================================

#ifdef HZ_DEBUG
#define HZ_ASSERT(expr, ...)                                                                       \
    do {                                                                                           \
        if (!(expr)) {                                                                             \
            HZ_ENGINE_FATAL("Assertion failed: {}", #expr);                                        \
            HZ_ENGINE_FATAL(__VA_ARGS__);                                                          \
            std::abort();                                                                          \
        }                                                                                          \
    } while (false)
#else
#define HZ_ASSERT(expr, ...) ((void)0)
#endif

#define HZ_VERIFY(expr, ...)                                                                       \
    do {                                                                                           \
        if (!(expr)) {                                                                             \
            HZ_ENGINE_FATAL("Verification failed: {}", #expr);                                     \
            HZ_ENGINE_FATAL(__VA_ARGS__);                                                          \
            std::abort();                                                                          \
        }                                                                                          \
    } while (false)
