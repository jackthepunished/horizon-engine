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

    # Renderer (OpenGL)
    renderer/renderer.cpp
    renderer/camera.cpp
    renderer/mesh.cpp
    renderer/opengl/shader.cpp
    renderer/opengl/buffer.cpp

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

    # Renderer (OpenGL)
    renderer/renderer.hpp
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
        glm::glm
        spdlog::spdlog
)

target_compile_definitions(horizon_engine
    PUBLIC
        GLM_FORCE_RADIANS
        GLM_FORCE_DEPTH_ZERO_TO_ONE
        $<$<BOOL:${HZ_HEADLESS}>:HZ_HEADLESS=1>
)

# Precompiled headers disabled due to C/C++ mixing with glad.c
# target_precompile_headers(horizon_engine
#     PRIVATE
#         <memory>
#         <vector>
#         <string>
#         <array>
#         <optional>
#         <span>
#         <memory_resource>
#         <cstdint>
# )
