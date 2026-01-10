#pragma once

/**
 * @file component_storage.hpp
 * @brief Sparse-set based component storage for the ECS
 *
 * Provides cache-friendly, contiguous storage for components with O(1) lookup.
 * Uses the sparse-set pattern for fast iteration and access.
 */

#include "engine/core/log.hpp"
#include "engine/core/types.hpp"
#include "entity.hpp"

#include <memory_resource>
#include <span>
#include <type_traits>
#include <vector>

namespace hz {

// ============================================================================
// Component Concepts
// ============================================================================

/**
 * @brief Concept for valid component types
 */
template <typename T>
concept Component =
    std::is_default_constructible_v<T> && std::is_move_constructible_v<T> && !std::is_pointer_v<T>;

// ============================================================================
// Component Storage Interface
// ============================================================================

/**
 * @brief Type-erased base class for component storage
 */
class IComponentStorage {
public:
    virtual ~IComponentStorage() = default;

    /**
     * @brief Check if an entity has a component in this storage
     */
    [[nodiscard]] virtual bool contains(Entity entity) const = 0;

    /**
     * @brief Remove a component from an entity
     */
    virtual void remove(Entity entity) = 0;

    /**
     * @brief Get the number of components stored
     */
    [[nodiscard]] virtual usize size() const = 0;

    /**
     * @brief Clear all components
     */
    virtual void clear() = 0;
};

// ============================================================================
// Typed Component Storage (Sparse Set)
// ============================================================================

/**
 * @brief Sparse-set based storage for a specific component type
 *
 * Uses two arrays:
 * - Sparse: Maps entity index -> dense index (or INVALID)
 * - Dense: Contiguous array of {entity, component} pairs
 *
 * This provides:
 * - O(1) add, remove, lookup
 * - Cache-friendly iteration over all components
 * - Stable iteration (swaps on remove)
 *
 * @tparam T The component type
 */
template <Component T>
class ComponentStorage : public IComponentStorage {
public:
    static constexpr u32 INVALID_INDEX = std::numeric_limits<u32>::max();

    explicit ComponentStorage(std::pmr::memory_resource* allocator = nullptr)
        : m_sparse(allocator ? allocator : std::pmr::get_default_resource())
        , m_dense_entities(allocator ? allocator : std::pmr::get_default_resource())
        , m_dense_components(allocator ? allocator : std::pmr::get_default_resource()) {}

    /**
     * @brief Add or replace a component for an entity
     */
    template <typename... Args>
    T& emplace(Entity entity, Args&&... args) {
        HZ_ASSERT(entity.is_valid(), "Cannot add component to invalid entity");

        // Ensure sparse array is large enough
        if (entity.index >= m_sparse.size()) {
            m_sparse.resize(entity.index + 1, INVALID_INDEX);
        }

        u32& sparse_idx = m_sparse[entity.index];

        if (sparse_idx != INVALID_INDEX) {
            // Replace existing component
            m_dense_components[sparse_idx] = T{std::forward<Args>(args)...};
            return m_dense_components[sparse_idx];
        }

        // Add new component
        sparse_idx = static_cast<u32>(m_dense_entities.size());
        m_dense_entities.push_back(entity);
        m_dense_components.emplace_back(std::forward<Args>(args)...);

        return m_dense_components.back();
    }

    /**
     * @brief Get a component for an entity (const)
     */
    [[nodiscard]] const T* get(Entity entity) const {
        if (!contains(entity)) {
            return nullptr;
        }
        return &m_dense_components[m_sparse[entity.index]];
    }

    /**
     * @brief Get a component for an entity (mutable)
     */
    [[nodiscard]] T* get(Entity entity) {
        if (!contains(entity)) {
            return nullptr;
        }
        return &m_dense_components[m_sparse[entity.index]];
    }

    /**
     * @brief Check if an entity has this component
     */
    [[nodiscard]] bool contains(Entity entity) const override {
        if (!entity.is_valid() || entity.index >= m_sparse.size()) {
            return false;
        }
        u32 dense_idx = m_sparse[entity.index];
        return dense_idx != INVALID_INDEX && dense_idx < m_dense_entities.size() &&
               m_dense_entities[dense_idx] == entity;
    }

    /**
     * @brief Remove a component from an entity
     */
    void remove(Entity entity) override {
        if (!contains(entity)) {
            return;
        }

        u32 dense_idx = m_sparse[entity.index];
        u32 last_idx = static_cast<u32>(m_dense_entities.size()) - 1;

        if (dense_idx != last_idx) {
            // Swap with last element
            Entity last_entity = m_dense_entities[last_idx];
            m_dense_entities[dense_idx] = last_entity;
            m_dense_components[dense_idx] = std::move(m_dense_components[last_idx]);
            m_sparse[last_entity.index] = dense_idx;
        }

        m_dense_entities.pop_back();
        m_dense_components.pop_back();
        m_sparse[entity.index] = INVALID_INDEX;
    }

    [[nodiscard]] usize size() const override { return m_dense_entities.size(); }

    void clear() override {
        m_sparse.clear();
        m_dense_entities.clear();
        m_dense_components.clear();
    }

    // ========================================================================
    // Iteration Support
    // ========================================================================

    /**
     * @brief Get a view of all entities with this component
     */
    [[nodiscard]] std::span<const Entity> entities() const { return m_dense_entities; }

    /**
     * @brief Get a view of all components
     */
    [[nodiscard]] std::span<T> components() { return m_dense_components; }

    [[nodiscard]] std::span<const T> components() const { return m_dense_components; }

    /**
     * @brief Iterator for (entity, component) pairs
     */
    class Iterator {
    public:
        using difference_type = std::ptrdiff_t;
        using value_type = std::pair<Entity, T&>;

        Iterator(ComponentStorage* storage, usize index) : m_storage(storage), m_index(index) {}

        std::pair<Entity, T&> operator*() const {
            return {m_storage->m_dense_entities[m_index], m_storage->m_dense_components[m_index]};
        }

        Iterator& operator++() {
            ++m_index;
            return *this;
        }
        Iterator operator++(int) {
            Iterator tmp = *this;
            ++m_index;
            return tmp;
        }

        bool operator==(const Iterator& other) const = default;

    private:
        ComponentStorage* m_storage;
        usize m_index;
    };

    Iterator begin() { return Iterator(this, 0); }
    Iterator end() { return Iterator(this, m_dense_entities.size()); }

private:
    std::pmr::vector<u32> m_sparse; // entity.index -> dense index
    std::pmr::vector<Entity> m_dense_entities;
    std::pmr::vector<T> m_dense_components;
};

} // namespace hz
