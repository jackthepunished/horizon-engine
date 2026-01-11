// Disable Jolt's Debug renderer in non-debug builds
#ifndef JPH_DEBUG_RENDERER
#define JPH_DEBUG_RENDERER 0
#endif

#include "physics_world.hpp"

#include <cstdlib>
#include <thread>

#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <glm/gtc/quaternion.hpp>

namespace hz {

// ============================================================================
// Jolt Memory Allocators
// ============================================================================

void* JoltAlloc(size_t size) {
    return std::malloc(size);
}

void JoltFree(void* ptr) {
    std::free(ptr);
}

void* JoltAlignedAlloc(size_t size, size_t alignment) {
    return std::aligned_alloc(alignment, size);
}

void JoltAlignedFree(void* ptr) {
    std::free(ptr);
}

// ============================================================================
// Broad Phase Layer Interface
// ============================================================================

class PhysicsWorld::BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface {
public:
    BPLayerInterfaceImpl() {
        m_object_to_broad_phase[PhysicsLayers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
        m_object_to_broad_phase[PhysicsLayers::MOVING] = BroadPhaseLayers::MOVING;
    }

    virtual unsigned int GetNumBroadPhaseLayers() const override {
        return BroadPhaseLayers::NUM_LAYERS;
    }

    virtual JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override {
        JPH_ASSERT(layer < PhysicsLayers::NUM_LAYERS);
        return m_object_to_broad_phase[layer];
    }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    virtual const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override {
        switch ((JPH::BroadPhaseLayer::Type)layer) {
        case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::NON_MOVING:
            return "NON_MOVING";
        case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::MOVING:
            return "MOVING";
        default:
            return "UNKNOWN";
        }
    }
#endif

private:
    JPH::BroadPhaseLayer m_object_to_broad_phase[PhysicsLayers::NUM_LAYERS];
};

// ============================================================================
// Object vs Broad Phase Layer Filter
// ============================================================================

class PhysicsWorld::ObjectVsBroadPhaseLayerFilterImpl : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
    virtual bool ShouldCollide(JPH::ObjectLayer layer1,
                               JPH::BroadPhaseLayer layer2) const override {
        switch (layer1) {
        case PhysicsLayers::NON_MOVING:
            return layer2 == BroadPhaseLayers::MOVING;
        case PhysicsLayers::MOVING:
            return true;
        default:
            JPH_ASSERT(false);
            return false;
        }
    }
};

// ============================================================================
// Object Layer Pair Filter
// ============================================================================

class PhysicsWorld::ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter {
public:
    virtual bool ShouldCollide(JPH::ObjectLayer layer1, JPH::ObjectLayer layer2) const override {
        switch (layer1) {
        case PhysicsLayers::NON_MOVING:
            return layer2 == PhysicsLayers::MOVING;
        case PhysicsLayers::MOVING:
            return true;
        default:
            JPH_ASSERT(false);
            return false;
        }
    }
};

// ============================================================================
// PhysicsWorld Implementation
// ============================================================================

PhysicsWorld::PhysicsWorld() = default;

PhysicsWorld::~PhysicsWorld() noexcept {
    shutdown();
}

bool PhysicsWorld::init() {
    if (m_initialized)
        return true;

    // Register Jolt allocators
    JPH::RegisterDefaultAllocator();

    // Create factory
    m_factory = std::make_unique<JPH::Factory>();
    JPH::Factory::sInstance = m_factory.get();

    // Register all types
    JPH::RegisterTypes();

    // Create temp allocator (10 MB)
    m_temp_allocator = std::make_unique<JPH::TempAllocatorImpl>(10 * 1024 * 1024);

    // Create job system with 1 thread per core (max 8)
    auto num_threads = std::min(static_cast<unsigned int>(std::thread::hardware_concurrency()), 8u);
    m_job_system = std::make_unique<JPH::JobSystemThreadPool>(
        JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, static_cast<int>(num_threads) - 1);

    // Create layer interfaces
    m_broad_phase_layer_interface = std::make_unique<BPLayerInterfaceImpl>();
    m_object_vs_broadphase_layer_filter = std::make_unique<ObjectVsBroadPhaseLayerFilterImpl>();
    m_object_layer_pair_filter = std::make_unique<ObjectLayerPairFilterImpl>();

    // Create physics system
    constexpr unsigned int max_bodies = 10240;
    constexpr unsigned int num_body_mutexes = 0; // Use default
    constexpr unsigned int max_body_pairs = 10240;
    constexpr unsigned int max_contact_constraints = 10240;

    m_physics_system = std::make_unique<JPH::PhysicsSystem>();
    m_physics_system->Init(max_bodies, num_body_mutexes, max_body_pairs, max_contact_constraints,
                           *m_broad_phase_layer_interface, *m_object_vs_broadphase_layer_filter,
                           *m_object_layer_pair_filter);

    // Set gravity
    m_physics_system->SetGravity(JPH::Vec3(0.0f, -9.81f, 0.0f));

    m_initialized = true;
    HZ_ENGINE_INFO("Physics system initialized ({} threads)", num_threads);
    return true;
}

void PhysicsWorld::shutdown() {
    if (!m_initialized)
        return;

    m_physics_system.reset();
    m_job_system.reset();
    m_temp_allocator.reset();
    m_object_layer_pair_filter.reset();
    m_object_vs_broadphase_layer_filter.reset();
    m_broad_phase_layer_interface.reset();

    // Destroy factory
    m_factory.reset();
    JPH::Factory::sInstance = nullptr;

    m_initialized = false;
    HZ_ENGINE_INFO("Physics system shutdown");
}

void PhysicsWorld::update(f32 delta_time) {
    if (!m_initialized)
        return;

    // Physics runs at fixed 60 Hz
    constexpr f32 physics_dt = 1.0f / 60.0f;
    constexpr int collision_steps = 1;

    m_physics_system->Update(std::min(delta_time, physics_dt), collision_steps,
                             m_temp_allocator.get(), m_job_system.get());
}

PhysicsBodyID PhysicsWorld::create_static_box(const glm::vec3& position,
                                              const glm::vec3& half_extents) {
    if (!m_initialized)
        return PhysicsBodyID::invalid();

    auto& body_interface = m_physics_system->GetBodyInterface();

    JPH::BoxShapeSettings box_settings(JPH::Vec3(half_extents.x, half_extents.y, half_extents.z));
    JPH::ShapeSettings::ShapeResult shape_result = box_settings.Create();

    if (shape_result.HasError()) {
        HZ_ENGINE_ERROR("Failed to create box shape: {}", shape_result.GetError().c_str());
        return PhysicsBodyID::invalid();
    }

    JPH::BodyCreationSettings body_settings(
        shape_result.Get(), JPH::RVec3(position.x, position.y, position.z), JPH::Quat::sIdentity(),
        JPH::EMotionType::Static, PhysicsLayers::NON_MOVING);

    JPH::BodyID body_id =
        body_interface.CreateAndAddBody(body_settings, JPH::EActivation::DontActivate);
    return {body_id};
}

PhysicsBodyID PhysicsWorld::create_dynamic_box(const glm::vec3& position,
                                               const glm::vec3& half_extents, f32 mass) {
    if (!m_initialized)
        return PhysicsBodyID::invalid();

    auto& body_interface = m_physics_system->GetBodyInterface();

    JPH::BoxShapeSettings box_settings(JPH::Vec3(half_extents.x, half_extents.y, half_extents.z));
    JPH::ShapeSettings::ShapeResult shape_result = box_settings.Create();

    if (shape_result.HasError()) {
        HZ_ENGINE_ERROR("Failed to create box shape: {}", shape_result.GetError().c_str());
        return PhysicsBodyID::invalid();
    }

    JPH::BodyCreationSettings body_settings(
        shape_result.Get(), JPH::RVec3(position.x, position.y, position.z), JPH::Quat::sIdentity(),
        JPH::EMotionType::Dynamic, PhysicsLayers::MOVING);
    body_settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
    body_settings.mMassPropertiesOverride.mMass = mass;

    JPH::BodyID body_id =
        body_interface.CreateAndAddBody(body_settings, JPH::EActivation::Activate);
    return {body_id};
}

PhysicsBodyID PhysicsWorld::create_dynamic_sphere(const glm::vec3& position, f32 radius, f32 mass) {
    if (!m_initialized)
        return PhysicsBodyID::invalid();

    auto& body_interface = m_physics_system->GetBodyInterface();

    JPH::SphereShapeSettings sphere_settings(radius);
    JPH::ShapeSettings::ShapeResult shape_result = sphere_settings.Create();

    if (shape_result.HasError()) {
        HZ_ENGINE_ERROR("Failed to create sphere shape: {}", shape_result.GetError().c_str());
        return PhysicsBodyID::invalid();
    }

    JPH::BodyCreationSettings body_settings(
        shape_result.Get(), JPH::RVec3(position.x, position.y, position.z), JPH::Quat::sIdentity(),
        JPH::EMotionType::Dynamic, PhysicsLayers::MOVING);
    body_settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
    body_settings.mMassPropertiesOverride.mMass = mass;

    JPH::BodyID body_id =
        body_interface.CreateAndAddBody(body_settings, JPH::EActivation::Activate);
    return {body_id};
}

void PhysicsWorld::remove_body(PhysicsBodyID body_id) {
    if (!m_initialized || !body_id.is_valid())
        return;

    auto& body_interface = m_physics_system->GetBodyInterface();
    body_interface.RemoveBody(body_id.id);
    body_interface.DestroyBody(body_id.id);
}

glm::vec3 PhysicsWorld::get_body_position(PhysicsBodyID body_id) const {
    if (!m_initialized || !body_id.is_valid())
        return glm::vec3(0.0f);

    auto& body_interface = m_physics_system->GetBodyInterface();
    JPH::RVec3 pos = body_interface.GetCenterOfMassPosition(body_id.id);
    return glm::vec3(pos.GetX(), pos.GetY(), pos.GetZ());
}

glm::quat PhysicsWorld::get_body_rotation(PhysicsBodyID body_id) const {
    if (!m_initialized || !body_id.is_valid())
        return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

    auto& body_interface = m_physics_system->GetBodyInterface();
    JPH::Quat rot = body_interface.GetRotation(body_id.id);
    return glm::quat(rot.GetW(), rot.GetX(), rot.GetY(), rot.GetZ());
}

void PhysicsWorld::set_body_position(PhysicsBodyID body_id, const glm::vec3& position) {
    if (!m_initialized || !body_id.is_valid())
        return;

    auto& body_interface = m_physics_system->GetBodyInterface();
    body_interface.SetPosition(body_id.id, JPH::RVec3(position.x, position.y, position.z),
                               JPH::EActivation::Activate);
}

void PhysicsWorld::set_body_velocity(PhysicsBodyID body_id, const glm::vec3& velocity) {
    if (!m_initialized || !body_id.is_valid())
        return;

    auto& body_interface = m_physics_system->GetBodyInterface();
    body_interface.SetLinearVelocity(body_id.id, JPH::Vec3(velocity.x, velocity.y, velocity.z));
}

void PhysicsWorld::apply_impulse(PhysicsBodyID body_id, const glm::vec3& impulse) {
    if (!m_initialized || !body_id.is_valid())
        return;

    auto& body_interface = m_physics_system->GetBodyInterface();
    body_interface.AddImpulse(body_id.id, JPH::Vec3(impulse.x, impulse.y, impulse.z));
}

RaycastHit PhysicsWorld::raycast(const glm::vec3& origin, const glm::vec3& direction,
                                 f32 max_distance) const {
    RaycastHit result;
    if (!m_initialized)
        return result;

    JPH::RRayCast ray(JPH::RVec3(origin.x, origin.y, origin.z),
                      JPH::Vec3(direction.x * max_distance, direction.y * max_distance,
                                direction.z * max_distance));

    JPH::RayCastResult hit;
    if (m_physics_system->GetNarrowPhaseQuery().CastRay(ray, hit)) {
        result.hit = true;
        result.distance = hit.mFraction * max_distance;

        JPH::RVec3 hit_pos = ray.GetPointOnRay(hit.mFraction);
        result.position = glm::vec3(hit_pos.GetX(), hit_pos.GetY(), hit_pos.GetZ());

        result.body_id = {hit.mBodyID};

        // Get normal from body
        auto& body_interface = m_physics_system->GetBodyInterface();
        JPH::Vec3 normal =
            body_interface.GetShape(hit.mBodyID)
                ->GetSurfaceNormal(hit.mSubShapeID2, ray.GetPointOnRay(hit.mFraction));
        result.normal = glm::vec3(normal.GetX(), normal.GetY(), normal.GetZ());
    }

    return result;
}

} // namespace hz
