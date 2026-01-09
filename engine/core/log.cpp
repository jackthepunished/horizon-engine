#include "log.hpp"

#include <spdlog/sinks/stdout_color_sinks.h>

namespace hz {

std::shared_ptr<spdlog::logger> Log::s_engine_logger;
std::shared_ptr<spdlog::logger> Log::s_app_logger;

namespace {

spdlog::level::level_enum to_spdlog_level(LogLevel level) {
    switch (level) {
    case LogLevel::Trace:
        return spdlog::level::trace;
    case LogLevel::Debug:
        return spdlog::level::debug;
    case LogLevel::Info:
        return spdlog::level::info;
    case LogLevel::Warn:
        return spdlog::level::warn;
    case LogLevel::Error:
        return spdlog::level::err;
    case LogLevel::Fatal:
        return spdlog::level::critical;
    case LogLevel::Off:
        return spdlog::level::off;
    }
    return spdlog::level::trace;
}

} // anonymous namespace

void Log::init(LogLevel engine_level, LogLevel app_level) {
    // Create color console sinks
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_pattern("%^[%T] [%n] %v%$");

    // Engine logger
    s_engine_logger = std::make_shared<spdlog::logger>("HORIZON", console_sink);
    s_engine_logger->set_level(to_spdlog_level(engine_level));
    s_engine_logger->flush_on(spdlog::level::warn);
    spdlog::register_logger(s_engine_logger);

    // Application logger
    s_app_logger = std::make_shared<spdlog::logger>("APP", console_sink);
    s_app_logger->set_level(to_spdlog_level(app_level));
    s_app_logger->flush_on(spdlog::level::warn);
    spdlog::register_logger(s_app_logger);

    HZ_ENGINE_INFO("Logging system initialized");
}

void Log::shutdown() {
    HZ_ENGINE_INFO("Logging system shutting down");
    spdlog::shutdown();
}

} // namespace hz
