/**
 * @file main.cpp
 * @brief Horizon Engine - Entry Point
 *
 * This is the main entry point for the Horizon Engine game.
 * The Application class handles all initialization, game loop, and cleanup.
 */

#include "application.hpp"

#include <exception>

#include <engine/core/log.hpp>

int main() {
    try {
        game::Application app;

        if (!app.init()) {
            return 1;
        }

        app.run();
        app.shutdown();

        return 0;
    } catch (const std::exception& e) {
        HZ_FATAL("Fatal error: {}", e.what());
        return 1;
    }
}
