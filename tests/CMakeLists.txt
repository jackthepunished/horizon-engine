# ============================================================================
# Horizon Engine Tests
# ============================================================================

# Catch2 provides its own CMake module
list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)
include(Catch)

# Unit tests executable
add_executable(horizon_tests
    unit/test_main.cpp
    unit/test_memory.cpp
    unit/test_game_loop.cpp
)

target_link_libraries(horizon_tests
    PRIVATE
        horizon_engine
        Catch2::Catch2WithMain
)

# Discover tests
catch_discover_tests(horizon_tests)
