// Disable Jolt's Debug renderer in non-debug builds
#ifndef JPH_DEBUG_RENDERER
#define JPH_DEBUG_RENDERER 0
#endif

#include "projectile_system.hpp"

#include "engine/core/log.hpp"
#include "engine/scene/components.hpp"

#include <algorithm>
#include <cmath>

namespace hz {

void ProjectileSystem::init(PhysicsWorld& physics_world, HitboxSystem& hitbox_system) {
    m_physics_world = &physics_world;
    m_hitbox_system = &hitbox_system;
    HZ_ENGINE_INFO("Projectile system initialized");
}

void ProjectileSystem::shutdown() {
    m_physics_world = nullptr;
    m_hitbox_system = nullptr;
    m_hit_callback = nullptr;
    m_explosion_callback = nullptr;
    HZ_ENGINE_INFO("Projectile system shutdown");
}

void ProjectileSystem::update(entt::registry& registry, f32 delta_time) {
    update_ballistic_projectiles(registry, delta_time);
    cleanup_destroyed_projectiles(registry);
}

HitscanResult ProjectileSystem::fire_hitscan(const glm::vec3& origin, const glm::vec3& direction,
                                             const ProjectileData& data, entt::entity owner,
                                             entt::registry& registry) {
    HitscanResult result;

    if (!m_physics_world || !m_hitbox_system) {
        return result;
    }

    // Normalize direction
    glm::vec3 dir = glm::normalize(direction);

    // Perform raycast
    RaycastHit hit;
    Hitbox* hitbox = nullptr;
    entt::entity hit_entity = entt::null;

    bool hit_something = m_hitbox_system->raycast_hitboxes(origin, dir, data.max_range, registry,
                                                           hit, hitbox, hit_entity);

    if (!hit_something) {
        result.hit = false;
        return result;
    }

    result.hit = true;
    result.hit_point = hit.position;
    result.hit_normal = hit.normal;
    result.distance = hit.distance;
    result.hit_entity = hit_entity;
    result.hit_hitbox = hitbox;

    if (hitbox) {
        result.hit_location = hitbox->type;
    }

    // Calculate damage with falloff
    result.raw_damage = data.base_damage;
    f32 falloff = calculate_damage_falloff(data, hit.distance);
    result.final_damage = data.base_damage * falloff;

    // Apply hitbox multiplier
    if (hitbox) {
        result.final_damage *= hitbox->damage_multiplier;
    }

    // Apply damage to target if it has a hurtbox
    if (hit_entity != entt::null && registry.valid(hit_entity)) {
        auto* hurtbox = registry.try_get<HurtboxComponent>(hit_entity);
        if (hurtbox) {
            glm::vec3 damage_dir = -dir;
            hurtbox->apply_damage(result.final_damage, result.hit_location, damage_dir, hitbox);
        }
    }

    // Fire hit callback
    if (m_hit_callback) {
        m_hit_callback(result);
    }

    // Handle penetration
    if (data.max_penetrations > 0 && data.penetration_power > 0.0f) {
        // TODO: Implement penetration by continuing raycast from exit point
    }

    return result;
}

entt::entity ProjectileSystem::spawn_ballistic(const glm::vec3& origin, const glm::vec3& direction,
                                               const ProjectileData& data, entt::entity owner,
                                               entt::registry& registry) {
    entt::entity entity = registry.create();

    auto& proj = registry.emplace<ProjectileComponent>(entity);
    proj.data = data;
    proj.position = origin;
    proj.start_position = origin;
    proj.velocity = glm::normalize(direction) * data.muzzle_velocity;
    proj.owner = owner;
    proj.time_alive = 0.0f;
    proj.distance_traveled = 0.0f;
    proj.penetration_count = 0;
    proj.pending_destroy = false;

    // Create physics body if needed (for more accurate collision)
    // For now, we use swept raycasts which is more efficient

    return entity;
}

f32 ProjectileSystem::calculate_damage_falloff(const ProjectileData& data, f32 distance) {
    if (distance <= data.damage_falloff_start) {
        return 1.0f;
    }

    if (distance >= data.damage_falloff_end) {
        return data.min_damage_multiplier;
    }

    // Linear interpolation
    f32 t = (distance - data.damage_falloff_start) /
            (data.damage_falloff_end - data.damage_falloff_start);
    return 1.0f - t * (1.0f - data.min_damage_multiplier);
}

void ProjectileSystem::update_ballistic_projectiles(entt::registry& registry, f32 delta_time) {
    auto view = registry.view<ProjectileComponent>();

    for (auto entity : view) {
        auto& proj = view.get<ProjectileComponent>(entity);

        if (proj.pending_destroy)
            continue;
        if (proj.data.type == ProjectileType::Hitscan)
            continue;

        // Store previous position for swept collision
        glm::vec3 prev_position = proj.position;

        // Apply gravity
        proj.velocity.y -= GRAVITY * proj.data.gravity_scale * delta_time;

        // Apply drag if any
        if (proj.data.drag_coefficient > 0.0f) {
            f32 speed = glm::length(proj.velocity);
            if (speed > 0.01f) {
                glm::vec3 drag_dir = -glm::normalize(proj.velocity);
                f32 drag_force = proj.data.drag_coefficient * speed * speed;
                proj.velocity += drag_dir * drag_force * delta_time;
            }
        }

        // Update position
        proj.position += proj.velocity * delta_time;
        proj.time_alive += delta_time;
        proj.distance_traveled += glm::length(proj.position - prev_position);

        // Check lifetime
        if (proj.time_alive >= proj.data.max_lifetime) {
            if (proj.data.explosive) {
                // Explode at current position (grenade fuse)
                process_explosion(registry, proj.position, proj.data);
            }
            proj.pending_destroy = true;
            continue;
        }

        // Swept collision (CCD)
        glm::vec3 move_dir = proj.position - prev_position;
        f32 move_dist = glm::length(move_dir);

        if (move_dist > 0.001f) {
            move_dir /= move_dist;

            RaycastHit hit;
            Hitbox* hitbox = nullptr;
            entt::entity hit_entity = entt::null;

            bool hit_something = m_hitbox_system->raycast_hitboxes(
                prev_position, move_dir, move_dist + 0.1f, registry, hit, hitbox, hit_entity);

            if (hit_something && hit.distance <= move_dist) {
                // Don't hit owner
                if (hit_entity != proj.owner) {
                    process_projectile_hit(registry, proj, hit, hitbox, hit_entity);
                }
            }
        }
    }
}

void ProjectileSystem::process_projectile_hit(entt::registry& registry, ProjectileComponent& proj,
                                              const RaycastHit& hit, Hitbox* hitbox,
                                              entt::entity hit_entity) {
    // Calculate damage
    f32 falloff = calculate_damage_falloff(proj.data, proj.distance_traveled);
    f32 damage = proj.data.base_damage * falloff;

    if (hitbox) {
        damage *= hitbox->damage_multiplier;
    }

    // Apply damage
    if (hit_entity != entt::null && registry.valid(hit_entity)) {
        auto* hurtbox = registry.try_get<HurtboxComponent>(hit_entity);
        if (hurtbox) {
            glm::vec3 damage_dir = glm::normalize(proj.velocity);
            HitboxType hit_location = hitbox ? hitbox->type : HitboxType::Torso;
            hurtbox->apply_damage(damage, hit_location, damage_dir, hitbox);
        }
    }

    // Fire hit callback
    if (m_hit_callback) {
        HitscanResult result;
        result.hit = true;
        result.hit_point = hit.position;
        result.hit_normal = hit.normal;
        result.distance = proj.distance_traveled;
        result.hit_entity = hit_entity;
        result.hit_hitbox = hitbox;
        result.hit_location = hitbox ? hitbox->type : HitboxType::Torso;
        result.raw_damage = proj.data.base_damage;
        result.final_damage = damage;
        m_hit_callback(result);
    }

    // Handle explosion
    if (proj.data.explosive) {
        process_explosion(registry, hit.position, proj.data);
    }

    // Mark for destruction
    proj.pending_destroy = true;
}

void ProjectileSystem::process_explosion(entt::registry& registry, const glm::vec3& position,
                                         const ProjectileData& data) {
    if (!data.explosive || data.explosion_radius <= 0.0f)
        return;

    // Fire explosion callback for visual effects
    if (m_explosion_callback) {
        m_explosion_callback(position, data.explosion_radius, data.explosion_damage);
    }

    // Apply explosion damage to nearby entities with hurtboxes
    auto view = registry.view<TransformComponent, HurtboxComponent>();

    for (auto entity : view) {
        auto& transform = view.get<TransformComponent>(entity);
        auto& hurtbox = view.get<HurtboxComponent>(entity);

        glm::vec3 to_target = transform.position - position;
        f32 distance = glm::length(to_target);

        if (distance > data.explosion_radius)
            continue;

        // Calculate damage based on distance from center
        f32 distance_factor = 1.0f - (distance / data.explosion_radius);
        distance_factor = std::pow(distance_factor, data.explosion_falloff);
        f32 damage = data.explosion_damage * distance_factor;

        // Apply damage
        glm::vec3 damage_dir = distance > 0.01f ? glm::normalize(to_target) : glm::vec3(0, 1, 0);
        hurtbox.apply_damage(damage, HitboxType::Torso, damage_dir, nullptr);
    }
}

void ProjectileSystem::cleanup_destroyed_projectiles(entt::registry& registry) {
    auto view = registry.view<ProjectileComponent>();

    std::vector<entt::entity> to_destroy;
    for (auto entity : view) {
        auto& proj = view.get<ProjectileComponent>(entity);
        if (proj.pending_destroy) {
            to_destroy.push_back(entity);
        }
    }

    for (auto entity : to_destroy) {
        registry.destroy(entity);
    }
}

} // namespace hz
