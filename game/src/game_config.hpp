#pragma once

/**
 * @file game_config.hpp
 * @brief Game configuration constants
 */

namespace GameConfig {

// =============================================================================
// Window Settings
// =============================================================================
constexpr int WINDOW_WIDTH = 1920;
constexpr int WINDOW_HEIGHT = 1080;
constexpr float ASPECT_RATIO = static_cast<float>(WINDOW_WIDTH) / static_cast<float>(WINDOW_HEIGHT);

// =============================================================================
// Player Movement
// =============================================================================
constexpr float MOVEMENT_SPEED = 5.0f;
constexpr float SPRINT_MULTIPLIER = 2.0f;
constexpr float CROUCH_MULTIPLIER = 0.5f;
constexpr float MOUSE_SENSITIVITY = 0.1f;

// =============================================================================
// Player Physics
// =============================================================================
constexpr float GRAVITY = -20.0f;
constexpr float JUMP_FORCE = 8.0f;
constexpr float GROUND_LEVEL = 1.6f;  // Player eye height (ground + 1.6m)
constexpr float CROUCH_HEIGHT = 0.8f; // Crouched eye height
constexpr float PLAYER_MASS = 80.0f;

// =============================================================================
// Player Collider
// =============================================================================
constexpr float PLAYER_CAPSULE_RADIUS = 0.4f;
constexpr float PLAYER_CAPSULE_HALF_HEIGHT = 0.5f;

// =============================================================================
// Shooting
// =============================================================================
constexpr float RAYCAST_MAX_DISTANCE = 100.0f;
constexpr float IMPULSE_STRENGTH = 50.0f;

// =============================================================================
// VFX
// =============================================================================
constexpr float IMPACT_VFX_LIFETIME = 2.0f;
constexpr float IMPACT_VFX_SIZE = 0.2f;

// =============================================================================
// Scene Settings
// =============================================================================
constexpr int PBR_GRID_ROWS = 7;
constexpr int PBR_GRID_COLS = 7;
constexpr float PBR_GRID_SPACING = 2.5f;

// =============================================================================
// Character Settings
// =============================================================================
constexpr float CHARACTER_EYE_OFFSET = 1.5f;     // How far below camera the character is
constexpr float CHARACTER_FORWARD_OFFSET = 0.4f; // How far in front of camera

} // namespace GameConfig
