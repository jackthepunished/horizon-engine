# ============================================================================
# Third-Party Dependencies via FetchContent
# ============================================================================

include(FetchContent)

set(FETCHCONTENT_QUIET OFF)

# ============================================================================
# GLFW - Windowing
# ============================================================================

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
    GIT_SHALLOW TRUE
)

# ============================================================================
# GLM - Math Library
# ============================================================================

FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
    GIT_SHALLOW TRUE
)

# ============================================================================
# spdlog - Logging
# ============================================================================

set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.14.1
    GIT_SHALLOW TRUE
)

# ============================================================================
# stb - Image Loading (header-only)
# ============================================================================

FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG f1c79c02822848a9bed4315b12c8c8f3761e1296  # Pinned for reproducible builds
    GIT_SHALLOW TRUE
)

# ============================================================================
# tinyobjloader - OBJ Model Loading (header-only)
# ============================================================================

FetchContent_Declare(
    tinyobjloader
    GIT_REPOSITORY https://github.com/tinyobjloader/tinyobjloader.git
    GIT_TAG v2.0.0rc13
    GIT_SHALLOW TRUE
)

# ============================================================================
# JoltPhysics - Physics Engine
# ============================================================================

set(USE_STATIC_MSVC_RUNTIME_LIBRARY OFF CACHE BOOL "" FORCE)
set(ENABLE_ALL_WARNINGS OFF CACHE BOOL "" FORCE)
set(INTERPROCEDURAL_OPTIMIZATION OFF CACHE BOOL "" FORCE)

# Suppress CMake warning about install() path normalization
if(POLICY CMP0177)
    set(CMAKE_POLICY_DEFAULT_CMP0177 NEW)
endif()

FetchContent_Declare(
    JoltPhysics
    GIT_REPOSITORY https://github.com/jrouwe/JoltPhysics.git
    GIT_TAG v5.0.0
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR Build
)

# ============================================================================
# Dear ImGui - UI Library
# ============================================================================

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.90.4
    GIT_SHALLOW TRUE
)

# ============================================================================
# miniaudio - Audio Engine
# ============================================================================

FetchContent_Declare(
    miniaudio
    GIT_REPOSITORY https://github.com/mackron/miniaudio.git
    GIT_TAG 0.11.21
    GIT_SHALLOW TRUE
)

# ============================================================================
# EnTT - Entity Component System (header-only)
# ============================================================================

FetchContent_Declare(
    entt
    GIT_REPOSITORY https://github.com/skypjack/entt.git
    GIT_TAG v3.13.0
    GIT_SHALLOW TRUE
)

# ============================================================================
# Catch2 - Testing (only if tests enabled)
# ============================================================================

if(HZ_BUILD_TESTS)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.5.2
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(Catch2)
endif()

# ============================================================================
# Make All Available
# ============================================================================

FetchContent_MakeAvailable(glfw spdlog stb tinyobjloader JoltPhysics imgui miniaudio entt)

# Manually setup GLM to avoid its deprecated CMakeLists.txt
FetchContent_GetProperties(glm)
if(NOT glm_POPULATED)
    if(POLICY CMP0169)
        cmake_policy(SET CMP0169 OLD)
    endif()
    FetchContent_Populate(glm)
    if(POLICY CMP0169)
        cmake_policy(SET CMP0169 NEW)
    endif()
    add_library(glm INTERFACE)
    target_include_directories(glm SYSTEM INTERFACE ${glm_SOURCE_DIR})
endif()

# Create interface libraries for header-only deps
add_library(stb INTERFACE)
target_include_directories(stb INTERFACE ${stb_SOURCE_DIR})

add_library(tinyobj INTERFACE)
target_include_directories(tinyobj INTERFACE ${tinyobjloader_SOURCE_DIR})

add_library(miniaudio_lib INTERFACE)
target_include_directories(miniaudio_lib INTERFACE ${miniaudio_SOURCE_DIR})

# ImGui library (needs to be built as a static library)
add_library(imgui_lib STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

target_include_directories(imgui_lib PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

target_link_libraries(imgui_lib PUBLIC glfw)
