/**
 * @file main.cpp
 * @brief Horizon Engine - Entry Point
 *
 * This is the main entry point for the Horizon Engine game.
 * The Application class handles all initialization, game loop, and cleanup.
 */

#include "application.hpp"

#include <clocale>
#include <exception>
#include <locale>

#include <engine/core/log.hpp>

int main() {
    // Fix for non-ASCII system locales (e.g., Turkish Windows OEM code page 857):
    // MinGW's std::filesystem::path::string() uses the C locale for narrow char conversion,
    // which can throw "Illegal byte sequence" if the locale doesn't support the characters.
    std::setlocale(LC_ALL, "C");
    try {
        std::locale::global(std::locale("C"));
    } catch (...) {
        // Fall back silently if locale setting fails
    }

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
