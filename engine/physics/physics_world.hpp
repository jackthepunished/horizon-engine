#pragma once

/**
 * @file physics_world.hpp
 * @brief Jolt Physics world wrapper
 */

#include "engine/core/log.hpp"
#include "engine/core/types.hpp"

// Jolt Physics - MUST include Jolt.h first!
#include <Jolt/Jolt.h>

// Now other Jolt headers
#include <memory>
#include <vector>

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>
#include <glm/glm.hpp>

namespace hz {

/**
 * @brief Physics layer definitions
 */
namespace PhysicsLayers {
static constexpr JPH::ObjectLayer NON_MOVING = 0;
static constexpr JPH::ObjectLayer MOVING = 1;
static constexpr JPH::ObjectLayer NUM_LAYERS = 2;
} // namespace PhysicsLayers

/**
 * @brief Broad phase layer definitions
 */
namespace BroadPhaseLayers {
static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
static constexpr JPH::BroadPhaseLayer MOVING(1);
static constexpr unsigned int NUM_LAYERS = 2;
} // namespace BroadPhaseLayers

/**
 * @brief Body ID wrapper for type safety
 */
struct PhysicsBodyID {
    JPH::BodyID id;

    [[nodiscard]] bool is_valid() const { return !id.IsInvalid(); }
    static PhysicsBodyID invalid() { return {JPH::BodyID()}; }
};

/**
 * @brief Raycast hit result
 */
struct RaycastHit {
    glm::vec3 position{0.0f};
    glm::vec3 normal{0.0f};
    f32 distance{0.0f};
    PhysicsBodyID body_id;
    bool hit{false};
};

/**
 * @brief Physics world - manages Jolt simulation
 */
class PhysicsWorld {
public:
    PhysicsWorld();
    ~PhysicsWorld();

    HZ_NON_COPYABLE(PhysicsWorld);
    HZ_NON_MOVABLE(PhysicsWorld);

    bool init();
    void shutdown();
    void update(f32 delta_time);

    PhysicsBodyID create_static_box(const glm::vec3& position, const glm::vec3& half_extents);
    PhysicsBodyID create_dynamic_box(const glm::vec3& position, const glm::vec3& half_extents,
                                     f32 mass = 1.0f);
    PhysicsBodyID create_dynamic_sphere(const glm::vec3& position, f32 radius, f32 mass = 1.0f);
    void remove_body(PhysicsBodyID body_id);

    [[nodiscard]] glm::vec3 get_body_position(PhysicsBodyID body_id) const;
    [[nodiscard]] glm::quat get_body_rotation(PhysicsBodyID body_id) const;
    void set_body_position(PhysicsBodyID body_id, const glm::vec3& position);
    void set_body_velocity(PhysicsBodyID body_id, const glm::vec3& velocity);
    void apply_impulse(PhysicsBodyID body_id, const glm::vec3& impulse);

    [[nodiscard]] RaycastHit raycast(const glm::vec3& origin, const glm::vec3& direction,
                                     f32 max_distance = 1000.0f) const;
    [[nodiscard]] JPH::PhysicsSystem* jolt_system() { return m_physics_system.get(); }

private:
    std::unique_ptr<JPH::TempAllocatorImpl> m_temp_allocator;
    std::unique_ptr<JPH::JobSystemThreadPool> m_job_system;
    std::unique_ptr<JPH::PhysicsSystem> m_physics_system;
    std::unique_ptr<JPH::Factory> m_factory;

    class BPLayerInterfaceImpl;
    class ObjectVsBroadPhaseLayerFilterImpl;
    class ObjectLayerPairFilterImpl;

    std::unique_ptr<BPLayerInterfaceImpl> m_broad_phase_layer_interface;
    std::unique_ptr<ObjectVsBroadPhaseLayerFilterImpl> m_object_vs_broadphase_layer_filter;
    std::unique_ptr<ObjectLayerPairFilterImpl> m_object_layer_pair_filter;

    bool m_initialized{false};
};

} // namespace hz
