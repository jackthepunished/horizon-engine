#pragma once

/**
 * @file physics_config.hpp
 * @brief Common physics configuration and Jolt setup macros
 *
 * Include this header BEFORE any Jolt headers in implementation files.
 * This ensures consistent configuration across all physics modules.
 */

#include <cstddef> // for size_t

// Disable Jolt's Debug renderer in non-debug builds
// This must be defined BEFORE including any Jolt headers
#ifndef JPH_DEBUG_RENDERER
#ifdef NDEBUG
#define JPH_DEBUG_RENDERER 0
#else
#define JPH_DEBUG_RENDERER 0 // Set to 1 to enable debug rendering in debug builds
#endif
#endif

// Physics system constants
namespace hz::PhysicsConfig {
// Simulation settings
constexpr int MAX_BODIES = 10240;
constexpr int MAX_BODY_PAIRS = 10240;
constexpr int MAX_CONTACT_CONSTRAINTS = 10240;

// Memory allocation
constexpr std::size_t TEMP_ALLOCATOR_SIZE = 10 * 1024 * 1024;      // 10 MB
constexpr std::size_t CHARACTER_TEMP_ALLOCATOR_SIZE = 1024 * 1024; // 1 MB

// Simulation
constexpr int COLLISION_STEPS = 1;
constexpr int INTEGRATION_SUBSTEPS = 1;
} // namespace hz::PhysicsConfig
