# ============================================================================
# Sample FPS Game
# ============================================================================

add_executable(horizon_game
    src/main.cpp
    src/editor_ui.cpp
    src/application.cpp
    src/systems/player_system.cpp
    src/systems/physics_system.cpp
    src/systems/animation_system.cpp
    src/systems/character_system.cpp
    src/systems/lifetime_system.cpp
)

target_link_libraries(horizon_game
    PRIVATE
        horizon_engine
)

target_include_directories(horizon_game
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
