#pragma once

/**
 * @file world.hpp
 * @brief ECS World - the central container for entities, components, and systems
 */

#include "component_storage.hpp"
#include "engine/core/log.hpp"
#include "engine/core/memory.hpp"
#include "engine/core/types.hpp"
#include "entity.hpp"
#include "system.hpp"

#include <algorithm>
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace hz {

// ============================================================================
// Entity Manager
// ============================================================================

/**
 * @brief Manages entity creation and destruction with generation tracking
 */
class EntityManager {
public:
    explicit EntityManager(std::pmr::memory_resource* allocator = nullptr);

    /**
     * @brief Create a new entity
     */
    [[nodiscard]] Entity create();

    /**
     * @brief Destroy an entity
     */
    void destroy(Entity entity);

    /**
     * @brief Check if an entity is still alive
     */
    [[nodiscard]] bool is_alive(Entity entity) const;

    /**
     * @brief Get the number of live entities
     */
    [[nodiscard]] usize count() const { return m_alive_count; }

    /**
     * @brief Clear all entities
     */
    void clear();

private:
    std::pmr::vector<u32> m_generations;
    std::pmr::vector<u32> m_free_indices;
    usize m_alive_count{0};
};

// ============================================================================
// World
// ============================================================================

/**
 * @brief The central ECS container
 *
 * Owns all entities, components, and systems. Provides the main
 * interface for game logic to interact with the ECS.
 */
class World {
public:
    explicit World(std::pmr::memory_resource* allocator = nullptr);
    ~World();

    HZ_NON_COPYABLE(World);
    HZ_DEFAULT_MOVABLE(World);

    // ========================================================================
    // Entity Management
    // ========================================================================

    /**
     * @brief Create a new entity
     */
    [[nodiscard]] Entity create_entity();

    /**
     * @brief Destroy an entity and all its components
     */
    void destroy_entity(Entity entity);

    /**
     * @brief Check if an entity is alive
     */
    [[nodiscard]] bool is_alive(Entity entity) const;

    /**
     * @brief Get the number of live entities
     */
    [[nodiscard]] usize entity_count() const { return m_entity_manager.count(); }

    // ========================================================================
    // Component Management
    // ========================================================================

    /**
     * @brief Add or replace a component on an entity
     */
    template <Component T, typename... Args>
    T& add_component(Entity entity, Args&&... args) {
        auto& storage = get_or_create_storage<T>();
        return storage.emplace(entity, std::forward<Args>(args)...);
    }

    /**
     * @brief Get a component from an entity
     */
    template <Component T>
    [[nodiscard]] T* get_component(Entity entity) {
        auto* storage = get_storage<T>();
        return storage ? storage->get(entity) : nullptr;
    }

    template <Component T>
    [[nodiscard]] const T* get_component(Entity entity) const {
        auto* storage = get_storage<T>();
        return storage ? storage->get(entity) : nullptr;
    }

    /**
     * @brief Check if an entity has a component
     */
    template <Component T>
    [[nodiscard]] bool has_component(Entity entity) const {
        auto* storage = get_storage<T>();
        return storage && storage->contains(entity);
    }

    /**
     * @brief Remove a component from an entity
     */
    template <Component T>
    void remove_component(Entity entity) {
        if (auto* storage = get_storage<T>()) {
            storage->remove(entity);
        }
    }

    /**
     * @brief Get storage for a component type (for iteration)
     */
    template <Component T>
    [[nodiscard]] ComponentStorage<T>* get_storage() {
        auto it = m_component_storages.find(std::type_index(typeid(T)));
        if (it == m_component_storages.end()) {
            return nullptr;
        }
        return static_cast<ComponentStorage<T>*>(it->second.get());
    }

    template <Component T>
    [[nodiscard]] const ComponentStorage<T>* get_storage() const {
        auto it = m_component_storages.find(std::type_index(typeid(T)));
        if (it == m_component_storages.end()) {
            return nullptr;
        }
        return static_cast<const ComponentStorage<T>*>(it->second.get());
    }

    // ========================================================================
    // System Management
    // ========================================================================

    /**
     * @brief Register a system
     */
    template <typename T, typename... Args>
    T& add_system(Args&&... args) {
        static_assert(std::is_base_of_v<ISystem, T>, "T must derive from ISystem");

        auto system = std::make_unique<T>(std::forward<Args>(args)...);
        T& ref = *system;

        m_systems.push_back(std::move(system));
        sort_systems();

        ref.on_register(*this);
        HZ_ENGINE_DEBUG("Registered system: {}", ref.name());

        return ref;
    }

    /**
     * @brief Update all systems
     */
    void update(f64 dt);

    /**
     * @brief Clear all entities, components, and systems
     */
    void clear();

private:
    template <Component T>
    ComponentStorage<T>& get_or_create_storage() {
        auto type_idx = std::type_index(typeid(T));
        auto it = m_component_storages.find(type_idx);

        if (it == m_component_storages.end()) {
            auto storage = std::make_unique<ComponentStorage<T>>(m_allocator);
            auto* ptr = storage.get();
            m_component_storages[type_idx] = std::move(storage);
            return *ptr;
        }

        return *static_cast<ComponentStorage<T>*>(it->second.get());
    }

    void sort_systems();
    void remove_entity_components(Entity entity);

    std::pmr::memory_resource* m_allocator;
    EntityManager m_entity_manager;
    std::unordered_map<std::type_index, std::unique_ptr<IComponentStorage>> m_component_storages;
    std::vector<std::unique_ptr<ISystem>> m_systems;
};

} // namespace hz
