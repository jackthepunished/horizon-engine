// Include physics config first (sets up JPH_DEBUG_RENDERER before Jolt headers)
#include "physics_interactions.hpp"

#include "engine/core/log.hpp"
#include "engine/scene/components.hpp"
#include "physics_config.hpp"

#include <algorithm>
#include <cmath>

#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace hz {

// ============================================================================
// DestructibleComponent
// ============================================================================

bool DestructibleComponent::apply_damage(f32 damage, const glm::vec3& hit_point,
                                         const glm::vec3& hit_direction) {
    if (is_destroyed)
        return false;

    // Apply material hardness
    f32 effective_damage = damage / material.hardness;
    current_health -= effective_damage;

    last_hit_point = hit_point;
    last_hit_direction = hit_direction;

    if (current_health <= 0.0f) {
        current_health = 0.0f;
        is_destroyed = true;
        return true;
    }

    return false;
}

// ============================================================================
// BulletPenetration
// ============================================================================

PenetrationResult BulletPenetration::check_penetration(const ProjectileData& projectile,
                                                       const RaycastHit& hit,
                                                       const PhysicsMaterial& material,
                                                       f32 current_damage) {

    PenetrationResult result;

    // Check if projectile can penetrate at all
    if (projectile.penetration_power <= 0.0f || projectile.max_penetrations == 0) {
        result.can_penetrate = false;
        return result;
    }

    // Simple penetration check: penetration power vs material resistance
    f32 penetration_chance = projectile.penetration_power - material.penetration_resistance;

    if (penetration_chance <= 0.0f) {
        result.can_penetrate = false;
        return result;
    }

    // Calculate exit point (simple: just add thickness along ray direction)
    // In reality, you'd do another raycast from inside
    result.can_penetrate = true;
    result.exit_distance = material.thickness;
    result.material = material;

    // Calculate remaining damage
    result.remaining_damage = calculate_exit_damage(current_damage, material);

    return result;
}

f32 BulletPenetration::calculate_exit_damage(f32 entry_damage, const PhysicsMaterial& material) {
    f32 remaining = entry_damage * (1.0f - material.damage_reduction);
    return std::max(remaining, 0.0f);
}

glm::vec3 BulletPenetration::calculate_exit_direction(const glm::vec3& entry_direction,
                                                      const glm::vec3& surface_normal,
                                                      const PhysicsMaterial& material) {

    // Add slight deviation based on material
    // Softer materials cause more deviation
    f32 deviation = (1.0f - material.penetration_resistance) * 0.1f;

    // Simple random deviation (in production, use proper random)
    glm::vec3 deviated = entry_direction;
    deviated.x += deviation * (static_cast<f32>(rand()) / RAND_MAX - 0.5f);
    deviated.y += deviation * (static_cast<f32>(rand()) / RAND_MAX - 0.5f);
    deviated.z += deviation * (static_cast<f32>(rand()) / RAND_MAX - 0.5f);

    return glm::normalize(deviated);
}

// ============================================================================
// PhysicsInteractionSystem
// ============================================================================

void PhysicsInteractionSystem::init(PhysicsWorld& physics_world) {
    m_physics_world = &physics_world;
    HZ_ENGINE_INFO("Physics interaction system initialized");
}

void PhysicsInteractionSystem::shutdown() {
    m_physics_world = nullptr;
    m_destruction_callback = nullptr;
    m_grab_callback = nullptr;
    m_throw_callback = nullptr;
    HZ_ENGINE_INFO("Physics interaction system shutdown");
}

void PhysicsInteractionSystem::update(entt::registry& registry, f32 delta_time) {
    update_grabbed_objects(registry, delta_time);
}

void PhysicsInteractionSystem::damage_destructible(entt::registry& registry, entt::entity entity,
                                                   f32 damage, const glm::vec3& hit_point,
                                                   const glm::vec3& hit_direction) {
    auto* destructible = registry.try_get<DestructibleComponent>(entity);
    if (!destructible)
        return;

    bool destroyed = destructible->apply_damage(damage, hit_point, hit_direction);

    // Check for stage transitions
    check_destruction_stages(registry, *destructible, entity);

    if (destroyed) {
        destroy_object(registry, entity);
    }
}

void PhysicsInteractionSystem::destroy_object(entt::registry& registry, entt::entity entity) {
    auto* destructible = registry.try_get<DestructibleComponent>(entity);
    if (!destructible)
        return;

    auto* transform = registry.try_get<TransformComponent>(entity);
    glm::vec3 position = transform ? transform->position : glm::vec3(0.0f);

    // Spawn debris
    if (destructible->spawn_debris) {
        spawn_debris(registry, position, *destructible);
    }

    // Fire callback
    if (m_destruction_callback) {
        m_destruction_callback(entity, position);
    }

    // Mark for destruction (or destroy immediately)
    // In a real implementation, you might want to defer this
    registry.destroy(entity);
}

void PhysicsInteractionSystem::check_destruction_stages(entt::registry& registry,
                                                        DestructibleComponent& destructible,
                                                        entt::entity entity) {
    f32 health_percent = destructible.current_health / destructible.max_health;

    for (size_t i = destructible.current_stage; i < destructible.stages.size(); ++i) {
        const auto& stage = destructible.stages[i];
        if (health_percent <= stage.health_threshold) {
            destructible.current_stage = static_cast<int>(i) + 1;

            // Switch model, play sound, spawn particles
            // (In a real implementation, you'd handle model switching here)
            HZ_ENGINE_DEBUG("Destructible {} entered stage {}", static_cast<u32>(entity),
                            destructible.current_stage);
        }
    }
}

void PhysicsInteractionSystem::spawn_debris(entt::registry& registry, const glm::vec3& position,
                                            const DestructibleComponent& destructible) {
    if (!m_physics_world)
        return;

    for (int i = 0; i < destructible.debris_count; ++i) {
        // Random direction for debris
        f32 theta = static_cast<f32>(rand()) / RAND_MAX * glm::two_pi<f32>();
        f32 phi = static_cast<f32>(rand()) / RAND_MAX * glm::pi<f32>();

        glm::vec3 dir(std::sin(phi) * std::cos(theta),
                      std::cos(phi) + 0.5f, // Bias upward
                      std::sin(phi) * std::sin(theta));

        // Spawn offset
        glm::vec3 spawn_pos = position + dir * 0.2f;

        // Create debris physics body
        PhysicsBodyID debris_body = m_physics_world->create_dynamic_sphere(spawn_pos, 0.1f, 0.5f);

        // Apply force
        glm::vec3 force = dir * destructible.debris_force;

        // Add random component
        force.x += (static_cast<f32>(rand()) / RAND_MAX - 0.5f) * 2.0f;
        force.z += (static_cast<f32>(rand()) / RAND_MAX - 0.5f) * 2.0f;

        m_physics_world->apply_impulse(debris_body, force);

        // Create debris entity
        entt::entity debris_entity = registry.create();
        registry.emplace<TransformComponent>(debris_entity, TransformComponent{spawn_pos});

        // Store body ID for cleanup (you'd want a DebrisComponent for this)
    }
}

// ============================================================================
// Grabbable Props
// ============================================================================

bool PhysicsInteractionSystem::try_grab(entt::registry& registry, entt::entity grabber,
                                        entt::entity prop) {
    auto* prop_comp = registry.try_get<PhysicsPropComponent>(prop);
    if (!prop_comp)
        return false;

    // Check if prop is grabbable
    if (prop_comp->interaction_type != InteractionType::Grab &&
        prop_comp->interaction_type != InteractionType::Throw &&
        prop_comp->interaction_type != InteractionType::Carry) {
        return false;
    }

    // Check if already grabbed
    if (prop_comp->is_grabbed)
        return false;

    // Check distance
    auto* grabber_transform = registry.try_get<TransformComponent>(grabber);
    auto* prop_transform = registry.try_get<TransformComponent>(prop);

    if (grabber_transform && prop_transform) {
        f32 distance = glm::length(grabber_transform->position - prop_transform->position);
        if (distance > prop_comp->grab_distance) {
            return false;
        }
    }

    // Grab successful
    prop_comp->is_grabbed = true;
    prop_comp->grabbed_by = grabber;

    // Add grabbed component
    auto& grabbed = registry.emplace_or_replace<GrabbedObjectComponent>(prop);
    grabbed.grabber = grabber;
    grabbed.grab_distance = prop_comp->grab_distance;

    if (m_grab_callback) {
        m_grab_callback(grabber, prop);
    }

    return true;
}

void PhysicsInteractionSystem::release_grab(entt::registry& registry, entt::entity grabber) {
    auto view = registry.view<PhysicsPropComponent, GrabbedObjectComponent>();

    for (auto entity : view) {
        auto& grabbed = view.get<GrabbedObjectComponent>(entity);
        if (grabbed.grabber == grabber) {
            auto& prop = view.get<PhysicsPropComponent>(entity);
            prop.is_grabbed = false;
            prop.grabbed_by = entt::null;

            registry.remove<GrabbedObjectComponent>(entity);
            break;
        }
    }
}

void PhysicsInteractionSystem::throw_prop(entt::registry& registry, entt::entity grabber,
                                          const glm::vec3& direction) {
    auto view = registry.view<PhysicsPropComponent, GrabbedObjectComponent, TransformComponent>();

    for (auto entity : view) {
        auto& grabbed = view.get<GrabbedObjectComponent>(entity);
        if (grabbed.grabber != grabber)
            continue;

        auto& prop = view.get<PhysicsPropComponent>(entity);
        auto& transform = view.get<TransformComponent>(entity);

        // Check if throwable
        if (prop.interaction_type != InteractionType::Throw) {
            release_grab(registry, grabber);
            return;
        }

        // Calculate throw velocity
        glm::vec3 throw_velocity = glm::normalize(direction) * prop.throw_force;

        // Create physics body if needed and apply impulse
        if (m_physics_world) {
            PhysicsBodyID body =
                m_physics_world->create_dynamic_box(transform.position, glm::vec3(0.2f), prop.mass);
            m_physics_world->set_body_velocity(body, throw_velocity);
        }

        // Release grab
        prop.is_grabbed = false;
        prop.grabbed_by = entt::null;
        registry.remove<GrabbedObjectComponent>(entity);

        if (m_throw_callback) {
            m_throw_callback(entity, throw_velocity);
        }

        break;
    }
}

void PhysicsInteractionSystem::update_grab_position(entt::registry& registry, entt::entity grabber,
                                                    const glm::vec3& target_position) {
    auto view = registry.view<GrabbedObjectComponent, TransformComponent>();

    for (auto entity : view) {
        auto& grabbed = view.get<GrabbedObjectComponent>(entity);
        if (grabbed.grabber != grabber)
            continue;

        auto& transform = view.get<TransformComponent>(entity);

        // Move object towards target with spring physics
        glm::vec3 delta = target_position + grabbed.grab_offset - transform.position;
        transform.position += delta * 0.2f; // Simple lerp, could use proper spring

        break;
    }
}

void PhysicsInteractionSystem::update_grabbed_objects(entt::registry& registry, f32 delta_time) {
    auto view = registry.view<GrabbedObjectComponent, TransformComponent>();

    for (auto entity : view) {
        auto& grabbed = view.get<GrabbedObjectComponent>(entity);

        // Check if grabber still exists
        if (!registry.valid(grabbed.grabber)) {
            auto* prop = registry.try_get<PhysicsPropComponent>(entity);
            if (prop) {
                prop->is_grabbed = false;
                prop->grabbed_by = entt::null;
            }
            registry.remove<GrabbedObjectComponent>(entity);
        }
    }
}

// ============================================================================
// Bullet Penetration Processing
// ============================================================================

std::vector<HitscanResult> PhysicsInteractionSystem::process_bullet_with_penetration(
    entt::registry& registry, const glm::vec3& origin, const glm::vec3& direction,
    const ProjectileData& projectile, entt::entity shooter) {

    std::vector<HitscanResult> results;

    if (!m_physics_world)
        return results;

    glm::vec3 current_origin = origin;
    glm::vec3 current_direction = glm::normalize(direction);
    f32 current_damage = projectile.base_damage;
    f32 remaining_range = projectile.max_range;
    u8 penetrations = 0;

    while (remaining_range > 0.0f && penetrations <= projectile.max_penetrations) {
        // Raycast
        RaycastHit hit =
            m_physics_world->raycast(current_origin, current_direction, remaining_range);

        if (!hit.hit)
            break;

        // Create result for this hit
        HitscanResult result;
        result.hit = true;
        result.hit_point = hit.position;
        result.hit_normal = hit.normal;
        result.distance = hit.distance;
        result.raw_damage = current_damage;

        // Try to get material from hit entity
        PhysicsMaterial material = Materials::concrete(); // Default

        // Look up entity from body ID (would need proper lookup)
        // For now, use default material

        // Apply damage falloff
        f32 total_distance = projectile.max_range - remaining_range + hit.distance;
        f32 falloff = ProjectileSystem::calculate_damage_falloff(projectile, total_distance);
        result.final_damage = current_damage * falloff;

        results.push_back(result);

        // Check for destructible hit
        // (Would need entity lookup to apply damage)

        // Check penetration
        if (penetrations < projectile.max_penetrations) {
            PenetrationResult pen =
                BulletPenetration::check_penetration(projectile, hit, material, current_damage);

            if (pen.can_penetrate) {
                // Move origin past the surface
                current_origin = hit.position + current_direction * (material.thickness + 0.01f);
                current_direction = BulletPenetration::calculate_exit_direction(
                    current_direction, hit.normal, material);
                current_damage = pen.remaining_damage;
                remaining_range -= hit.distance + material.thickness;
                ++penetrations;
                continue;
            }
        }

        // No more penetration, stop
        break;
    }

    return results;
}

} // namespace hz
