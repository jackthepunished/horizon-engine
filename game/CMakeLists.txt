# ============================================================================
# Sample FPS Game
# ============================================================================

add_executable(horizon_game
    src/main.cpp
)

target_link_libraries(horizon_game
    PRIVATE
        horizon_engine
)

target_include_directories(horizon_game
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
