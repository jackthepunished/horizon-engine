#pragma once

/**
 * @file types.hpp
 * @brief Core type definitions for the Horizon Engine
 *
 * Provides fixed-width integer types, floating point aliases,
 * and strongly-typed handle primitives.
 */

#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace hz {

// ============================================================================
// Fixed-Width Integer Types
// ============================================================================

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

using i8 = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

using usize = std::size_t;
using isize = std::ptrdiff_t;

// ============================================================================
// Floating Point Types
// ============================================================================

using f32 = float;
using f64 = double;

static_assert(sizeof(f32) == 4, "f32 must be 4 bytes");
static_assert(sizeof(f64) == 8, "f64 must be 8 bytes");

// ============================================================================
// Handle Types
// ============================================================================

/**
 * @brief Strongly-typed handle template for type-safe resource references
 *
 * Handles provide a safe way to reference engine resources without raw pointers.
 * The Tag type parameter ensures compile-time type safety between different
 * handle types (e.g., EntityHandle vs TextureHandle).
 *
 * @tparam Tag Phantom type for compile-time differentiation
 * @tparam T Underlying integer type for the handle value
 */
template <typename Tag, typename T = u32>
struct Handle {
    static constexpr T INVALID_VALUE = std::numeric_limits<T>::max();

    T value{INVALID_VALUE};

    constexpr Handle() = default;
    constexpr explicit Handle(T v) : value(v) {}

    [[nodiscard]] constexpr bool is_valid() const noexcept {
        return value != INVALID_VALUE;
    }

    [[nodiscard]] constexpr explicit operator bool() const noexcept {
        return is_valid();
    }

    [[nodiscard]] constexpr bool operator==(const Handle& other) const noexcept = default;
    [[nodiscard]] constexpr auto operator<=>(const Handle& other) const noexcept = default;
};

/**
 * @brief Generational handle with embedded generation counter
 *
 * Provides safety against use-after-free by including a generation counter.
 * When a resource is freed and its slot reused, the generation increments,
 * invalidating old handles.
 */
template <typename Tag>
struct GenerationalHandle {
    static constexpr u32 INVALID_INDEX = std::numeric_limits<u32>::max();
    static constexpr u32 INVALID_GENERATION = 0;

    u32 index{INVALID_INDEX};
    u32 generation{INVALID_GENERATION};

    constexpr GenerationalHandle() = default;
    constexpr GenerationalHandle(u32 idx, u32 gen) : index(idx), generation(gen) {}

    [[nodiscard]] constexpr bool is_valid() const noexcept {
        return index != INVALID_INDEX && generation != INVALID_GENERATION;
    }

    [[nodiscard]] constexpr explicit operator bool() const noexcept {
        return is_valid();
    }

    [[nodiscard]] constexpr bool operator==(const GenerationalHandle& other) const noexcept = default;
};

// ============================================================================
// Common Constants
// ============================================================================

inline constexpr f64 PI = 3.14159265358979323846;
inline constexpr f64 TAU = 2.0 * PI;
inline constexpr f64 EPSILON = 1e-6;

// ============================================================================
// Utility Macros
// ============================================================================

#define HZ_UNUSED(x) (void)(x)

#define HZ_NON_COPYABLE(ClassName)             \
    ClassName(const ClassName&) = delete;      \
    ClassName& operator=(const ClassName&) = delete

#define HZ_NON_MOVABLE(ClassName)              \
    ClassName(ClassName&&) = delete;           \
    ClassName& operator=(ClassName&&) = delete

#define HZ_DEFAULT_MOVABLE(ClassName)          \
    ClassName(ClassName&&) noexcept = default; \
    ClassName& operator=(ClassName&&) noexcept = default

} // namespace hz
