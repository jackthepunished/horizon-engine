#pragma once

#include "engine/core/types.hpp"

#include <entt/entt.hpp>

namespace hz {

/**
 * @brief Aliases for EnTT types
 */
using Entity = entt::entity;

/**
 * @brief Represents a game scene containing entities and components
 */
class Scene {
public:
    Scene() = default;
    ~Scene() = default;

    /**
     * @brief Create a new entity
     */
    Entity create_entity() { return m_registry.create(); }

    /**
     * @brief Destroy an entity
     */
    void destroy_entity(Entity entity) { m_registry.destroy(entity); }

    /**
     * @brief Get the underlying registry
     */
    entt::registry& registry() { return m_registry; }
    const entt::registry& registry() const { return m_registry; }

    /**
     * @brief Clear the scene
     */
    void clear() { m_registry.clear(); }

    /**
     * @brief Get active entity count
     */
    size_t entity_count() const {
        // EnTT storage<Entity>() returns a pointer in some versions/const contexts
        if (auto* storage = m_registry.storage<Entity>()) {
            return storage->size();
        }
        return 0;
    }

    /**
     * @brief Check if entity is valid
     */
    bool is_valid(Entity entity) const { return m_registry.valid(entity); }

private:
    entt::registry m_registry;
};

} // namespace hz
