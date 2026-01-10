#include "world.hpp"

#include <algorithm>
#include <ranges>

namespace hz {

// ============================================================================
// EntityManager Implementation
// ============================================================================

EntityManager::EntityManager(std::pmr::memory_resource* allocator)
    : m_generations(allocator ? allocator : std::pmr::get_default_resource())
    , m_free_indices(allocator ? allocator : std::pmr::get_default_resource()) {}

Entity EntityManager::create() {
    u32 index;
    u32 generation;

    if (!m_free_indices.empty()) {
        // Reuse a freed slot
        index = m_free_indices.back();
        m_free_indices.pop_back();
        generation = m_generations[index];
    } else {
        // Allocate new slot
        index = static_cast<u32>(m_generations.size());
        m_generations.push_back(1); // Generation starts at 1
        generation = 1;
    }

    ++m_alive_count;
    return Entity{index, generation};
}

void EntityManager::destroy(Entity entity) {
    if (!is_alive(entity)) {
        return;
    }

    // Increment generation to invalidate existing handles
    ++m_generations[entity.index];
    m_free_indices.push_back(entity.index);
    --m_alive_count;
}

bool EntityManager::is_alive(Entity entity) const {
    if (!entity.is_valid() || entity.index >= m_generations.size()) {
        return false;
    }
    return m_generations[entity.index] == entity.generation;
}

void EntityManager::clear() {
    m_generations.clear();
    m_free_indices.clear();
    m_alive_count = 0;
}

// ============================================================================
// World Implementation
// ============================================================================

World::World(std::pmr::memory_resource* allocator)
    : m_allocator(allocator ? allocator : std::pmr::get_default_resource())
    , m_entity_manager(m_allocator) {
    HZ_ENGINE_DEBUG("World created");
}

World::~World() {
    clear();
    HZ_ENGINE_DEBUG("World destroyed");
}

Entity World::create_entity() {
    return m_entity_manager.create();
}

void World::destroy_entity(Entity entity) {
    if (!is_alive(entity)) {
        return;
    }

    remove_entity_components(entity);
    m_entity_manager.destroy(entity);
}

bool World::is_alive(Entity entity) const {
    return m_entity_manager.is_alive(entity);
}

void World::update(f64 dt) {
    for (const auto& system : m_systems) {
        system->update(*this, dt);
    }
}

void World::clear() {
    // Unregister systems
    for (const auto& system : m_systems) {
        system->on_unregister(*this);
    }
    m_systems.clear();

    // Clear components
    for (const auto& [type, storage] : m_component_storages) {
        storage->clear();
    }
    m_component_storages.clear();

    // Clear entities
    m_entity_manager.clear();
}

void World::sort_systems() {
    std::ranges::sort(m_systems,
                      [](const auto& a, const auto& b) { return a->priority() < b->priority(); });
}

void World::remove_entity_components(Entity entity) {
    for (const auto& [type, storage] : m_component_storages) {
        storage->remove(entity);
    }
}

} // namespace hz
