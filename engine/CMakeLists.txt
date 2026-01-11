# JSON
FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(json)

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

    # ECS (Deprecated - To be removed)
    # ecs/entity_manager.cpp
    # ecs/world.cpp

    # Scene
    scene/scene.cpp
    scene/components.cpp
    scene/scene_serializer.cpp

    # Assets
    assets/texture.cpp
    assets/model.cpp
    assets/asset_registry.cpp
    assets/cubemap.cpp

    # Physics
    physics/physics_world.cpp

    # UI
    ui/imgui_layer.cpp
    ui/debug_overlay.cpp

    # Renderer (OpenGL)
    renderer/renderer.cpp
    renderer/camera.cpp
    renderer/mesh.cpp
    renderer/ibl.cpp
    renderer/terrain.cpp
    renderer/grass.cpp
    renderer/opengl/buffer.cpp
    renderer/opengl/framebuffer.cpp
    renderer/opengl/shader.cpp

    # Audio
    audio/audio_engine.cpp

    # Animation
    animation/skeleton.cpp

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
    # ECS (Deprecated)
    # ecs/entity.hpp
    # ecs/component_storage.hpp
    # ecs/system.hpp
    # ecs/world.hpp
    
    # Scene
    scene/scene.hpp

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
    renderer/ibl.hpp
    renderer/terrain.hpp
    renderer/grass.hpp
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
        EnTT::EnTT
        glfw
        glm
        spdlog::spdlog
        nlohmann_json::nlohmann_json
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
