#pragma once

/**
 * @file memory.hpp
 * @brief Memory management for the Horizon Engine
 *
 * Provides PMR-based memory allocators for different engine subsystems:
 * - Frame arena: Reset every frame, for temporary allocations
 * - Persistent pool: Long-lived allocations
 * - Subsystem pools: Isolated pools per subsystem
 */

#include "log.hpp"
#include "types.hpp"

#include <array>
#include <memory>
#include <memory_resource>
#include <span>
#include <vector>

namespace hz {

// ============================================================================
// Memory Constants
// ============================================================================

inline constexpr usize FRAME_ARENA_SIZE = 16 * 1024 * 1024;  // 16 MB per frame
inline constexpr usize DEFAULT_POOL_SIZE = 64 * 1024 * 1024; // 64 MB default

// ============================================================================
// Linear Arena Allocator
// ============================================================================

/**
 * @brief Fast linear allocator that resets each frame
 *
 * Allocations are bump-pointer only, deallocations are no-ops.
 * The entire arena is reset at once via reset().
 */
class LinearArena : public std::pmr::memory_resource {
public:
    explicit LinearArena(usize capacity);
    ~LinearArena() override;

    HZ_NON_COPYABLE(LinearArena);
    LinearArena(LinearArena&& other) noexcept;
    LinearArena& operator=(LinearArena&& other) noexcept;

    /**
     * @brief Reset the arena, invalidating all allocations
     */
    void reset() noexcept;

    /**
     * @brief Get current allocation offset
     */
    [[nodiscard]] usize used() const noexcept { return m_offset; }

    /**
     * @brief Get total capacity
     */
    [[nodiscard]] usize capacity() const noexcept { return m_buffer.size(); }

    /**
     * @brief Get percentage of arena used
     */
    [[nodiscard]] f32 usage_percent() const noexcept {
        return !m_buffer.empty() ? static_cast<f32>(m_offset) / static_cast<f32>(m_buffer.size())
                                 : 0.0f;
    }

protected:
    void* do_allocate(usize bytes, usize alignment) override;
    void do_deallocate(void* p, usize bytes, usize alignment) override;
    [[nodiscard]] bool do_is_equal(const memory_resource& other) const noexcept override;

private:
    std::vector<std::byte> m_buffer;
    usize m_offset{0};
};

// ============================================================================
// Memory Domain
// ============================================================================

/**
 * @brief Identifies different memory domains for tracking and isolation
 */
enum class MemoryDomain : u8 {
    Frame,     // Per-frame temporary allocations
    ECS,       // Entity-component-system storage
    Renderer,  // GPU resource staging
    Assets,    // Loaded asset data
    Audio,     // Audio buffers
    Physics,   // Physics simulation data
    Scripting, // Script runtime data
    General    // General purpose
};

// ============================================================================
// Memory Context
// ============================================================================

/**
 * @brief Global memory context providing allocators for different domains
 */
class MemoryContext {
public:
    /**
     * @brief Initialize the memory context with default allocator sizes
     */
    static void init();

    /**
     * @brief Shutdown and release all memory
     */
    static void shutdown();

    /**
     * @brief Reset frame-temporary allocations
     */
    static void reset_frame();

    /**
     * @brief Get allocator for a specific domain
     */
    [[nodiscard]] static std::pmr::memory_resource* get(MemoryDomain domain);

    /**
     * @brief Get the frame arena allocator
     */
    [[nodiscard]] static LinearArena& frame_arena();

    /**
     * @brief Log memory statistics
     */
    static void log_stats();

private:
    static std::unique_ptr<LinearArena> s_frame_arena;
    static std::pmr::synchronized_pool_resource s_general_pool;
    static bool s_initialized;
};

// ============================================================================
// Scoped Arena Marker
// ============================================================================

/**
 * @brief RAII marker for sub-allocations within an arena
 *
 * Records the arena offset on construction and restores it on destruction,
 * effectively freeing all allocations made within the scope.
 */
class ScopedArenaMarker {
public:
    explicit ScopedArenaMarker(LinearArena& arena);
    ~ScopedArenaMarker();

    HZ_NON_COPYABLE(ScopedArenaMarker);
    HZ_NON_MOVABLE(ScopedArenaMarker);

private:
    LinearArena& m_arena;
    usize m_marker;
};

// ============================================================================
// PMR Type Aliases
// ============================================================================

template <typename T>
using PmrVector = std::pmr::vector<T>;

template <typename T>
using PmrString = std::pmr::basic_string<T>;

using PmrWString = std::pmr::wstring;

} // namespace hz
