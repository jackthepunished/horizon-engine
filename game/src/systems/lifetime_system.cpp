/**
 * @file lifetime_system.cpp
 * @brief Implementation of entity lifetime management
 */

#include "lifetime_system.hpp"

#include <vector>

namespace game {

void LifetimeSystem::update(hz::Scene& scene, float dt) {
    auto view = scene.registry().view<hz::LifetimeComponent>();

    std::vector<hz::Entity> to_destroy;

    for (auto [entity, lifetime] : view.each()) {
        lifetime.time_remaining -= dt;
        if (lifetime.time_remaining <= 0.0f) {
            to_destroy.push_back(entity);
        }
    }

    for (auto entity : to_destroy) {
        scene.destroy_entity(entity);
    }
}

} // namespace game
