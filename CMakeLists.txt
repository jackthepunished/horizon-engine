cmake_minimum_required(VERSION 3.21)

project(HorizonEngine
    VERSION 0.1.0
    DESCRIPTION "Modern C++20 3D FPS Game Engine"
    LANGUAGES C CXX
)

# ============================================================================
# Global Settings
# ============================================================================

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ============================================================================
# Build Options
# ============================================================================

option(HZ_BUILD_TESTS "Build unit tests" ON)
option(HZ_BUILD_GAME "Build sample game" ON)
option(HZ_ENABLE_VALIDATION "Enable Vulkan validation layers" ON)
option(HZ_HEADLESS "Build in headless mode (no GPU/window)" OFF)

# ============================================================================
# Compiler Warnings
# ============================================================================

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    add_compile_options(
        -Wall -Wextra -Wpedantic
        -Wno-unused-parameter
        -Wconversion
        -Wshadow
        -Wnon-virtual-dtor
    )
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-g -O0)
        add_compile_definitions(HZ_DEBUG=1)
    else()
        add_compile_options(-O3)
        add_compile_definitions(HZ_RELEASE=1)
    endif()
elseif(MSVC)
    add_compile_options(/W4 /WX /permissive-)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_definitions(HZ_DEBUG=1)
    else()
        add_compile_definitions(HZ_RELEASE=1)
    endif()
endif()

# ============================================================================
# Dependencies
# ============================================================================

add_subdirectory(third_party)

# ============================================================================
# Engine Library
# ============================================================================

add_subdirectory(engine)

# ============================================================================
# Sample Game
# ============================================================================

if(HZ_BUILD_GAME)
    add_subdirectory(game)
endif()

# ============================================================================
# Tests
# ============================================================================

if(HZ_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
