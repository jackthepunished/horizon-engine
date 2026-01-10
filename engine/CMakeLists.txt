# ============================================================================
# Horizon Engine Library
# ============================================================================

set(ENGINE_SOURCES
    # Core
    core/log.cpp
    core/memory.cpp
    core/game_loop.cpp

    # Platform
    platform/window.cpp
    platform/input.cpp

    # ECS
    ecs/entity_manager.cpp
    ecs/world.cpp

    # Assets
    assets/texture.cpp
    assets/model.cpp
    assets/asset_registry.cpp

    # Physics
    physics/physics_world.cpp

    # UI
    ui/imgui_layer.cpp
    ui/debug_overlay.cpp

    # Renderer (OpenGL)
    renderer/renderer.cpp
    renderer/camera.cpp
    renderer/mesh.cpp
    renderer/opengl/shader.cpp
    renderer/opengl/buffer.cpp # Re-added buffer.cpp

    # Audio
    audio/audio_engine.cpp

    # GLAD (OpenGL loader)
    vendor/glad/glad.c
)

set(ENGINE_HEADERS
    # Core
    core/types.hpp
    core/log.hpp
    core/memory.hpp
    core/game_loop.hpp

    # Platform
    platform/platform.hpp
    platform/window.hpp
    platform/input.hpp

    # ECS
    ecs/entity.hpp
    ecs/component_storage.hpp
    ecs/system.hpp
    ecs/world.hpp

    # Assets
    assets/asset_handle.hpp
    assets/texture.hpp
    assets/model.hpp
    assets/asset_registry.hpp

    # Physics
    physics/physics_world.hpp

    # UI
    ui/imgui_layer.hpp
    ui/debug_overlay.hpp

    # Renderer (OpenGL)
    renderer/renderer.hpp
    renderer/camera.hpp
    renderer/mesh.hpp
    renderer/opengl/gl_context.hpp
    renderer/opengl/shader.hpp
    renderer/opengl/buffer.hpp

    # GLAD
    vendor/glad/glad.h
)

add_library(horizon_engine STATIC ${ENGINE_SOURCES} ${ENGINE_HEADERS})

target_include_directories(horizon_engine
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor
)

target_link_libraries(horizon_engine
    PUBLIC
        glfw
        glm
        spdlog::spdlog
        imgui_lib
        miniaudio_lib
        Jolt
    PRIVATE
        stb
        tinyobj
)

target_compile_definitions(horizon_engine
    PUBLIC
        GLM_FORCE_RADIANS
        GLM_FORCE_DEPTH_ZERO_TO_ONE
        $<$<BOOL:${HZ_HEADLESS}>:HZ_HEADLESS=1>
)
