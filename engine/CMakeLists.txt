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
    physics/fps_character_controller.cpp
    physics/hitbox_system.cpp
    physics/projectile_system.cpp
    physics/physics_interactions.cpp

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
    renderer/water.cpp
    renderer/particle_system.cpp
    renderer/billboard.cpp
    renderer/opengl/buffer.cpp
    renderer/opengl/framebuffer.cpp
    renderer/opengl/shader.cpp
    renderer/opengl/uniform_buffer.cpp
    renderer/deferred_renderer.cpp
    renderer/debug_renderer.cpp
    renderer/cinematic_camera.cpp

    # Audio
    audio/audio_engine.cpp

    # Animation
    animation/skeleton.cpp
    animation/ik_solver.cpp
    animation/animation_blender.cpp

    # GLAD (OpenGL loader)
    vendor/glad/glad.c
    vendor/ufbx/ufbx.c
)

# RHI Vulkan Backend Sources (conditionally added)
set(ENGINE_VULKAN_SOURCES
    rhi/vulkan/vk_device.cpp
    rhi/vulkan/vk_buffer.cpp
    rhi/vulkan/vk_texture.cpp
    rhi/vulkan/vk_sampler.cpp
    rhi/vulkan/vk_sync.cpp
    rhi/vulkan/vk_pipeline.cpp
    rhi/vulkan/vk_descriptor.cpp
    rhi/vulkan/vk_command_list.cpp
    rhi/vulkan/vk_swapchain.cpp
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

    # Scene
    scene/scene.hpp

    # Assets
    assets/asset_handle.hpp
    assets/texture.hpp
    assets/model.hpp
    assets/asset_registry.hpp

    # Physics
    physics/physics_config.hpp
    physics/physics_world.hpp
    physics/fps_character_controller.hpp
    physics/hitbox_system.hpp
    physics/projectile_system.hpp
    physics/physics_interactions.hpp

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
    renderer/water.hpp
    renderer/particle_system.hpp
    renderer/billboard.hpp
    renderer/opengl/gl_context.hpp
    renderer/opengl/shader.hpp
    renderer/opengl/buffer.hpp
    renderer/opengl/uniform_buffer.hpp

    # RHI (Render Hardware Interface)
    rhi/rhi.hpp
    rhi/rhi_types.hpp
    rhi/rhi_resources.hpp
    rhi/rhi_pipeline.hpp
    rhi/rhi_descriptor.hpp
    rhi/rhi_command_list.hpp
    rhi/rhi_device.hpp

    # RHI Vulkan Backend
    rhi/vulkan/vk_common.hpp
    rhi/vulkan/vk_device.hpp
    rhi/vulkan/vk_buffer.hpp
    rhi/vulkan/vk_texture.hpp
    rhi/vulkan/vk_sampler.hpp
    rhi/vulkan/vk_sync.hpp
    rhi/vulkan/vk_pipeline.hpp
    rhi/vulkan/vk_descriptor.hpp
    rhi/vulkan/vk_command_list.hpp
    rhi/vulkan/vk_swapchain.hpp

    # GLAD
    vendor/glad/glad.h
    vendor/ufbx/ufbx.h
)

# Conditionally add Vulkan sources if Vulkan is available
if(HZ_HAS_VULKAN)
    list(APPEND ENGINE_SOURCES ${ENGINE_VULKAN_SOURCES})
endif()

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

# Link Vulkan dependencies if available
if(HZ_HAS_VULKAN)
    target_link_libraries(horizon_engine
        PUBLIC
            volk
        PRIVATE
            vma
    )
endif()

target_compile_definitions(horizon_engine
    PUBLIC
        GLM_FORCE_RADIANS
        GLM_FORCE_DEPTH_ZERO_TO_ONE
        $<$<BOOL:${HZ_HEADLESS}>:HZ_HEADLESS=1>
        $<$<BOOL:${HZ_HAS_VULKAN}>:HZ_VULKAN_BACKEND=1>
)
