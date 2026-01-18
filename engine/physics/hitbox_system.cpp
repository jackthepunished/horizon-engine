// Include physics config first (sets up JPH_DEBUG_RENDERER before Jolt headers)
#include "hitbox_system.hpp"

#include "engine/core/log.hpp"
#include "engine/scene/components.hpp"
#include "physics_config.hpp"

#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace hz {

// ============================================================================
// HitboxComponent
// ============================================================================

HitboxComponent HitboxComponent::create_humanoid() {
    HitboxComponent comp;

    // Head - sphere at top
    Hitbox head;
    head.name = "head";
    head.type = HitboxType::Head;
    head.shape = HitboxShape::Sphere;
    head.offset = glm::vec3(0.0f, 1.6f, 0.0f);
    head.dimensions = glm::vec3(0.15f, 0.0f, 0.0f); // radius
    head.damage_multiplier = 2.0f;
    comp.hitboxes.push_back(head);

    // Torso - capsule
    Hitbox torso;
    torso.name = "torso";
    torso.type = HitboxType::Torso;
    torso.shape = HitboxShape::Capsule;
    torso.offset = glm::vec3(0.0f, 1.1f, 0.0f);
    torso.dimensions = glm::vec3(0.25f, 0.4f, 0.0f); // radius, half-height
    torso.damage_multiplier = 1.0f;
    comp.hitboxes.push_back(torso);

    // Left Arm - capsule
    Hitbox left_arm;
    left_arm.name = "left_arm";
    left_arm.type = HitboxType::LeftArm;
    left_arm.shape = HitboxShape::Capsule;
    left_arm.offset = glm::vec3(-0.35f, 1.2f, 0.0f);
    left_arm.dimensions = glm::vec3(0.08f, 0.25f, 0.0f);
    left_arm.damage_multiplier = 0.75f;
    comp.hitboxes.push_back(left_arm);

    // Right Arm - capsule
    Hitbox right_arm;
    right_arm.name = "right_arm";
    right_arm.type = HitboxType::RightArm;
    right_arm.shape = HitboxShape::Capsule;
    right_arm.offset = glm::vec3(0.35f, 1.2f, 0.0f);
    right_arm.dimensions = glm::vec3(0.08f, 0.25f, 0.0f);
    right_arm.damage_multiplier = 0.75f;
    comp.hitboxes.push_back(right_arm);

    // Left Leg - capsule
    Hitbox left_leg;
    left_leg.name = "left_leg";
    left_leg.type = HitboxType::LeftLeg;
    left_leg.shape = HitboxShape::Capsule;
    left_leg.offset = glm::vec3(-0.15f, 0.45f, 0.0f);
    left_leg.dimensions = glm::vec3(0.1f, 0.35f, 0.0f);
    left_leg.damage_multiplier = 0.75f;
    comp.hitboxes.push_back(left_leg);

    // Right Leg - capsule
    Hitbox right_leg;
    right_leg.name = "right_leg";
    right_leg.type = HitboxType::RightLeg;
    right_leg.shape = HitboxShape::Capsule;
    right_leg.offset = glm::vec3(0.15f, 0.45f, 0.0f);
    right_leg.dimensions = glm::vec3(0.1f, 0.35f, 0.0f);
    right_leg.damage_multiplier = 0.75f;
    comp.hitboxes.push_back(right_leg);

    // Bone names for skeletal animation (optional)
    comp.bone_names = {"Head", "Spine2", "LeftArm", "RightArm", "LeftUpLeg", "RightUpLeg"};

    return comp;
}

// ============================================================================
// HurtboxComponent
// ============================================================================

f32 HurtboxComponent::apply_damage(f32 base_damage, HitboxType hit_location,
                                   const glm::vec3& damage_direction, const Hitbox* hitbox) {
    if (invulnerable || is_dead) {
        return 0.0f;
    }

    // Apply hitbox damage multiplier
    f32 multiplier =
        hitbox ? hitbox->damage_multiplier : get_default_damage_multiplier(hit_location);
    f32 modified_damage = base_damage * multiplier;

    // Apply armor
    f32 absorbed_by_armor = 0.0f;
    if (armor > 0.0f) {
        absorbed_by_armor = modified_damage * armor_effectiveness;
        absorbed_by_armor = std::min(absorbed_by_armor, armor);
        armor -= absorbed_by_armor;
        armor = std::max(armor, 0.0f);
    }

    f32 actual_damage = modified_damage - absorbed_by_armor;
    current_health -= actual_damage;

    // Store last damage info
    last_damage_amount = actual_damage;
    last_damage_direction = damage_direction;
    last_hit_location = hit_location;

    // Check for death
    if (current_health <= 0.0f) {
        current_health = 0.0f;
        is_dead = true;
    }

    return actual_damage;
}

void HurtboxComponent::heal(f32 amount) {
    if (is_dead)
        return;

    current_health = std::min(current_health + amount, max_health);
}

void HurtboxComponent::add_armor(f32 amount) {
    armor = std::min(armor + amount, max_armor);
}

// ============================================================================
// HitboxSystem
// ============================================================================

void HitboxSystem::init(PhysicsWorld& physics_world) {
    m_physics_world = &physics_world;
    HZ_ENGINE_INFO("Hitbox system initialized");
}

void HitboxSystem::shutdown() {
    m_body_to_hitbox.clear();
    m_physics_world = nullptr;
    HZ_ENGINE_INFO("Hitbox system shutdown");
}

void HitboxSystem::update(entt::registry& registry) {
    if (!m_physics_world)
        return;

    auto view = registry.view<TransformComponent, HitboxComponent>();

    for (auto entity : view) {
        auto& transform = view.get<TransformComponent>(entity);
        auto& hitbox_comp = view.get<HitboxComponent>(entity);

        for (auto& hitbox : hitbox_comp.hitboxes) {
            if (!hitbox.enabled || !hitbox.body_id.is_valid())
                continue;

            // Calculate world position of hitbox
            glm::vec3 world_pos = transform.position + hitbox.offset;
            m_physics_world->set_body_position(hitbox.body_id, world_pos);
        }
    }
}

void HitboxSystem::create_hitbox_bodies(entt::entity entity, HitboxComponent& hitbox_comp,
                                        const glm::vec3& world_position) {
    if (!m_physics_world)
        return;

    JPH::PhysicsSystem* physics_system = m_physics_world->jolt_system();
    if (!physics_system)
        return;

    auto& body_interface = physics_system->GetBodyInterface();

    for (size_t i = 0; i < hitbox_comp.hitboxes.size(); ++i) {
        auto& hitbox = hitbox_comp.hitboxes[i];

        // Create shape based on hitbox shape type
        JPH::ShapeSettings::ShapeResult shape_result;

        switch (hitbox.shape) {
        case HitboxShape::Sphere: {
            JPH::SphereShapeSettings settings(hitbox.dimensions.x);
            shape_result = settings.Create();
            break;
        }
        case HitboxShape::Capsule: {
            JPH::CapsuleShapeSettings settings(hitbox.dimensions.y, hitbox.dimensions.x);
            shape_result = settings.Create();
            break;
        }
        case HitboxShape::Box: {
            JPH::BoxShapeSettings settings(
                JPH::Vec3(hitbox.dimensions.x, hitbox.dimensions.y, hitbox.dimensions.z));
            shape_result = settings.Create();
            break;
        }
        }

        if (shape_result.HasError()) {
            HZ_ENGINE_ERROR("Failed to create hitbox shape: {}", shape_result.GetError().c_str());
            continue;
        }

        glm::vec3 hitbox_world_pos = world_position + hitbox.offset;

        // Create as kinematic body (moved by code, not physics)
        JPH::BodyCreationSettings body_settings(
            shape_result.Get(),
            JPH::RVec3(hitbox_world_pos.x, hitbox_world_pos.y, hitbox_world_pos.z),
            JPH::Quat::sIdentity(), JPH::EMotionType::Kinematic, PhysicsLayers::MOVING);

        // Set as sensor (no physics response, only detection)
        body_settings.mIsSensor = true;

        JPH::BodyID body_id =
            body_interface.CreateAndAddBody(body_settings, JPH::EActivation::Activate);

        hitbox.body_id = {body_id};

        // Register in lookup map
        m_body_to_hitbox[body_id.GetIndexAndSequenceNumber()] = {entity, i};
    }
}

void HitboxSystem::destroy_hitbox_bodies(HitboxComponent& hitbox_comp) {
    if (!m_physics_world)
        return;

    for (auto& hitbox : hitbox_comp.hitboxes) {
        if (hitbox.body_id.is_valid()) {
            // Remove from lookup
            m_body_to_hitbox.erase(hitbox.body_id.id.GetIndexAndSequenceNumber());

            // Remove physics body
            m_physics_world->remove_body(hitbox.body_id);
            hitbox.body_id = PhysicsBodyID::invalid();
        }
    }
}

bool HitboxSystem::raycast_hitboxes(const glm::vec3& origin, const glm::vec3& direction,
                                    f32 max_distance, entt::registry& registry, RaycastHit& out_hit,
                                    Hitbox*& out_hitbox, entt::entity& out_entity) {
    if (!m_physics_world) {
        return false;
    }

    // Perform raycast
    RaycastHit hit = m_physics_world->raycast(origin, direction, max_distance);

    if (!hit.hit) {
        return false;
    }

    // Look up which hitbox was hit
    auto it = m_body_to_hitbox.find(hit.body_id.id.GetIndexAndSequenceNumber());
    if (it == m_body_to_hitbox.end()) {
        // Hit something that's not a hitbox
        out_hit = hit;
        out_hitbox = nullptr;
        out_entity = entt::null;
        return true;
    }

    // Found a hitbox hit
    out_entity = it->second.first;
    size_t hitbox_index = it->second.second;

    if (registry.valid(out_entity)) {
        auto* hitbox_comp = registry.try_get<HitboxComponent>(out_entity);
        if (hitbox_comp && hitbox_index < hitbox_comp->hitboxes.size()) {
            out_hitbox = &hitbox_comp->hitboxes[hitbox_index];
        }
    }

    out_hit = hit;
    return true;
}

} // namespace hz
