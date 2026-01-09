#pragma once

/**
 * @file system.hpp
 * @brief System interface for the ECS
 *
 * Systems contain all game logic and operate on components.
 * They are executed in a deterministic order each frame.
 */

#include "engine/core/types.hpp"

#include <functional>
#include <string_view>

namespace hz {

// Forward declarations
class World;

// ============================================================================
// System Interface
// ============================================================================

/**
 * @brief Base class for all ECS systems
 *
 * Systems contain all game logic and operate on entities/components.
 * They must be stateless or manage their state carefully.
 */
class ISystem {
public:
    virtual ~ISystem() = default;

    /**
     * @brief Get the system's name for debugging/profiling
     */
    [[nodiscard]] virtual std::string_view name() const = 0;

    /**
     * @brief Called once when the system is registered
     */
    virtual void on_register(World& world) { HZ_UNUSED(world); }

    /**
     * @brief Called once before the system is unregistered
     */
    virtual void on_unregister(World& world) { HZ_UNUSED(world); }

    /**
     * @brief Update the system (called each fixed timestep)
     * @param world The ECS world
     * @param dt Fixed delta time in seconds
     */
    virtual void update(World& world, f64 dt) = 0;

    /**
     * @brief Get the system's execution priority (lower = earlier)
     */
    [[nodiscard]] virtual i32 priority() const { return 0; }
};

// ============================================================================
// System Priority Constants
// ============================================================================

namespace SystemPriority {
inline constexpr i32 INPUT = -1000;
inline constexpr i32 PHYSICS = -500;
inline constexpr i32 GAMEPLAY = 0;
inline constexpr i32 ANIMATION = 500;
inline constexpr i32 RENDERING = 1000;
} // namespace SystemPriority

// ============================================================================
// Lambda System Wrapper
// ============================================================================

/**
 * @brief Wraps a lambda/function into a system for simple cases
 */
class LambdaSystem : public ISystem {
public:
    using UpdateFn = std::function<void(World&, f64)>;

    LambdaSystem(std::string_view name, UpdateFn update_fn, i32 priority = 0)
        : m_name(name), m_update_fn(std::move(update_fn)), m_priority(priority) {}

    [[nodiscard]] std::string_view name() const override { return m_name; }
    [[nodiscard]] i32 priority() const override { return m_priority; }

    void update(World& world, f64 dt) override { m_update_fn(world, dt); }

private:
    std::string_view m_name;
    UpdateFn m_update_fn;
    i32 m_priority;
};

} // namespace hz
