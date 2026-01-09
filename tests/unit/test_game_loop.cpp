/**
 * @file test_game_loop.cpp
 * @brief Unit tests for the game loop
 */

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <engine/core/game_loop.hpp>
#include <engine/core/log.hpp>

using namespace hz;

// ============================================================================
// Game Loop Tests
// ============================================================================

TEST_CASE("GameLoop fixed timestep", "[core][gameloop]") {
    // Logger required for GameLoop
    Log::init(LogLevel::Off, LogLevel::Off);

    SECTION("Configuration defaults") {
        GameLoopConfig config;
        REQUIRE(config.fixed_timestep == Catch::Approx(1.0 / 60.0));
    }

    SECTION("Fixed timestep value") {
        GameLoopConfig config;
        config.fixed_timestep = 0.01; // 100 Hz
        config.log_fps = false;

        GameLoop loop(config);
        REQUIRE(loop.fixed_timestep() == Catch::Approx(0.01));
    }

    Log::shutdown();
}
