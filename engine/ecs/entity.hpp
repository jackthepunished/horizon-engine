#pragma once

/**
 * @file entity.hpp
 * @brief Entity management for the ECS
 *
 * Entities are opaque identifiers composed of an index and generation counter.
 * The generation ensures safe handle reuse after entity destruction.
 */

#include "engine/core/types.hpp"

#include <limits>

namespace hz {

// ============================================================================
// Entity Definition
// ============================================================================

/**
 * @brief Opaque entity identifier with generational safety
 *
 * An entity is a simple ID that can have components attached to it.
 * The index is used for storage lookup, the generation prevents use-after-free.
 *
 * Memory layout: [32-bit index][32-bit generation]
 */
struct Entity {
    static constexpr u32 INVALID_INDEX = std::numeric_limits<u32>::max();
    static constexpr u32 INVALID_GENERATION = 0;

    u32 index{INVALID_INDEX};
    u32 generation{INVALID_GENERATION};

    constexpr Entity() = default;
    constexpr Entity(u32 idx, u32 gen) : index(idx), generation(gen) {}

    /**
     * @brief Check if this entity handle is potentially valid
     */
    [[nodiscard]] constexpr bool is_valid() const noexcept {
        return index != INVALID_INDEX && generation != INVALID_GENERATION;
    }

    [[nodiscard]] constexpr explicit operator bool() const noexcept { return is_valid(); }

    [[nodiscard]] constexpr bool operator==(const Entity& other) const noexcept = default;

    /**
     * @brief Convert to a single 64-bit value for hashing/comparison
     */
    [[nodiscard]] constexpr u64 to_id() const noexcept {
        return (static_cast<u64>(generation) << 32) | static_cast<u64>(index);
    }

    /**
     * @brief Create from a 64-bit ID
     */
    [[nodiscard]] static constexpr Entity from_id(u64 id) noexcept {
        return Entity{static_cast<u32>(id & 0xFFFFFFFF), static_cast<u32>(id >> 32)};
    }
};

/**
 * @brief Invalid entity constant
 */
inline constexpr Entity NULL_ENTITY{};

} // namespace hz

// ============================================================================
// Hash Support
// ============================================================================

template <>
struct std::hash<hz::Entity> {
    std::size_t operator()(const hz::Entity& e) const noexcept {
        return std::hash<hz::u64>{}(e.to_id());
    }
};
