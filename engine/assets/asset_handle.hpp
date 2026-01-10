#pragma once

/**
 * @file asset_handle.hpp
 * @brief Type-safe asset handle with generation counter
 */

#include "engine/core/types.hpp"

#include <functional>

namespace hz {

/**
 * @brief Generation-based handle to an asset
 *
 * Prevents dangling references by tracking generation count.
 * When an asset is reloaded, its generation increases.
 */
template <typename T>
struct AssetHandle {
    u32 index{0};
    u32 generation{0};

    [[nodiscard]] bool is_valid() const noexcept { return index != 0 || generation != 0; }

    [[nodiscard]] bool operator==(const AssetHandle& other) const noexcept {
        return index == other.index && generation == other.generation;
    }

    [[nodiscard]] bool operator!=(const AssetHandle& other) const noexcept {
        return !(*this == other);
    }

    static AssetHandle invalid() { return {0, 0}; }
};

// Common handle types
class Texture;
class Model;

using TextureHandle = AssetHandle<Texture>;
using ModelHandle = AssetHandle<Model>;

} // namespace hz

// Hash support for handles
template <typename T>
struct std::hash<hz::AssetHandle<T>> {
    std::size_t operator()(const hz::AssetHandle<T>& h) const noexcept {
        return std::hash<hz::u64>{}((static_cast<hz::u64>(h.index) << 32) | h.generation);
    }
};
