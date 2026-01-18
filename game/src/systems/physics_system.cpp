/**
 * @file physics_system.cpp
 * @brief Implementation of physics body management and synchronization
 */

#include "physics_system.hpp"

#include <engine/core/log.hpp>
#include <glm/gtc/quaternion.hpp>

namespace game {

void PhysicsSystem::init(hz::PhysicsWorld& physics) {
    physics.init();
    HZ_LOG_INFO("PhysicsSystem initialized");
}

void PhysicsSystem::update(hz::Scene& scene, hz::PhysicsWorld& physics, float dt) {
    physics.update(dt);
    create_physics_bodies(scene, physics);
    sync_physics_to_ecs(scene, physics);
}

void PhysicsSystem::create_physics_bodies(hz::Scene& scene, hz::PhysicsWorld& physics) {
    // Create box collider bodies
    {
        auto view =
            scene.registry()
                .view<hz::TransformComponent, hz::RigidBodyComponent, hz::BoxColliderComponent>();

        for (auto [entity, tc, rb, bc] : view.each()) {
            if (rb.created) {
                continue;
            }

            hz::PhysicsBodyID body_id;
            if (rb.type == hz::RigidBodyComponent::BodyType::Static) {
                body_id = physics.create_static_box(tc.position, bc.half_extents);
            } else {
                body_id = physics.create_dynamic_box(tc.position, bc.half_extents, rb.mass);
            }

            if (body_id.is_valid()) {
                rb.set_body_id(new hz::PhysicsBodyID(body_id));
                rb.created = true;
            }
        }
    }

    // Create capsule collider bodies (for player, etc.)
    {
        auto view = scene.registry()
                        .view<hz::TransformComponent, hz::RigidBodyComponent,
                              hz::CapsuleColliderComponent>();

        for (auto [entity, tc, rb, cc] : view.each()) {
            if (rb.created) {
                continue;
            }

            // Note: PhysicsWorld may need a create_capsule method
            // For now, we skip capsule creation as it's not used for player movement
            // (player uses simple ground-check based movement)
        }
    }
}

void PhysicsSystem::sync_physics_to_ecs(hz::Scene& scene, hz::PhysicsWorld& physics) {
    auto view = scene.registry().view<hz::TransformComponent, hz::RigidBodyComponent>();

    for (auto [entity, tc, rb] : view.each()) {
        // Only sync dynamic bodies that have been created
        if (!rb.created || !rb.runtime_body ||
            rb.type != hz::RigidBodyComponent::BodyType::Dynamic) {
            continue;
        }

        auto* body_id_ptr = rb.get_body_id<hz::PhysicsBodyID>();
        if (!body_id_ptr) {
            continue;
        }

        // Update position
        tc.position = physics.get_body_position(*body_id_ptr);

        // Convert quaternion to Euler angles
        glm::quat q = physics.get_body_rotation(*body_id_ptr);
        tc.rotation = glm::degrees(glm::eulerAngles(q));
    }
}

} // namespace game
