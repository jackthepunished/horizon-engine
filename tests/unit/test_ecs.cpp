/**
 * @file test_ecs.cpp
 * @brief Unit tests for the Entity Component System
 */

#include <catch2/catch_test_macros.hpp>
#include <engine/core/log.hpp>
#include <engine/ecs/component_storage.hpp>
#include <engine/ecs/entity.hpp>
#include <engine/ecs/world.hpp>

using namespace hz;

// ============================================================================
// Test Components
// ============================================================================

struct Position {
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};
};

struct Velocity {
    float dx{0.0f};
    float dy{0.0f};
    float dz{0.0f};
};

struct Health {
    int current{100};
    int max{100};
};

// ============================================================================
// Entity Tests
// ============================================================================

TEST_CASE("Entity creation and validation", "[ecs][entity]") {
    SECTION("Default entity is invalid") {
        Entity e;
        REQUIRE_FALSE(e.is_valid());
        REQUIRE(e.index == Entity::INVALID_INDEX);
    }

    SECTION("Constructed entity is valid") {
        Entity e{5, 1};
        REQUIRE(e.is_valid());
        REQUIRE(e.index == 5);
        REQUIRE(e.generation == 1);
    }

    SECTION("NULL_ENTITY is invalid") {
        REQUIRE_FALSE(NULL_ENTITY.is_valid());
    }

    SECTION("Entity to/from ID") {
        Entity e{42, 7};
        u64 id = e.to_id();
        Entity restored = Entity::from_id(id);
        REQUIRE(restored == e);
    }
}

// ============================================================================
// Component Storage Tests
// ============================================================================

TEST_CASE("ComponentStorage basic operations", "[ecs][storage]") {
    ComponentStorage<Position> storage;

    SECTION("Empty storage") {
        REQUIRE(storage.size() == 0);
        REQUIRE_FALSE(storage.contains(Entity{0, 1}));
    }

    SECTION("Add and retrieve component") {
        Entity e{0, 1};
        auto& pos = storage.emplace(e, 1.0f, 2.0f, 3.0f);

        REQUIRE(storage.size() == 1);
        REQUIRE(storage.contains(e));
        REQUIRE(pos.x == 1.0f);
        REQUIRE(pos.y == 2.0f);
        REQUIRE(pos.z == 3.0f);

        auto* retrieved = storage.get(e);
        REQUIRE(retrieved != nullptr);
        REQUIRE(retrieved->x == 1.0f);
    }

    SECTION("Remove component") {
        Entity e{0, 1};
        storage.emplace(e);
        REQUIRE(storage.contains(e));

        storage.remove(e);
        REQUIRE_FALSE(storage.contains(e));
        REQUIRE(storage.size() == 0);
    }

    SECTION("Multiple entities") {
        Entity e1{0, 1};
        Entity e2{1, 1};
        Entity e3{2, 1};

        storage.emplace(e1, 1.0f, 0.0f, 0.0f);
        storage.emplace(e2, 2.0f, 0.0f, 0.0f);
        storage.emplace(e3, 3.0f, 0.0f, 0.0f);

        REQUIRE(storage.size() == 3);
        REQUIRE(storage.get(e1)->x == 1.0f);
        REQUIRE(storage.get(e2)->x == 2.0f);
        REQUIRE(storage.get(e3)->x == 3.0f);
    }

    SECTION("Swap on remove maintains integrity") {
        Entity e1{0, 1};
        Entity e2{1, 1};
        Entity e3{2, 1};

        storage.emplace(e1, 1.0f, 0.0f, 0.0f);
        storage.emplace(e2, 2.0f, 0.0f, 0.0f);
        storage.emplace(e3, 3.0f, 0.0f, 0.0f);

        // Remove middle element
        storage.remove(e2);

        REQUIRE(storage.size() == 2);
        REQUIRE(storage.contains(e1));
        REQUIRE_FALSE(storage.contains(e2));
        REQUIRE(storage.contains(e3));
        REQUIRE(storage.get(e1)->x == 1.0f);
        REQUIRE(storage.get(e3)->x == 3.0f);
    }
}

// ============================================================================
// World Tests
// ============================================================================

TEST_CASE("World entity management", "[ecs][world]") {
    Log::init(LogLevel::Off, LogLevel::Off);
    World world;

    SECTION("Create entities") {
        Entity e1 = world.create_entity();
        Entity e2 = world.create_entity();

        REQUIRE(e1.is_valid());
        REQUIRE(e2.is_valid());
        REQUIRE(e1 != e2);
        REQUIRE(world.entity_count() == 2);
    }

    SECTION("Destroy entities") {
        Entity e = world.create_entity();
        REQUIRE(world.is_alive(e));

        world.destroy_entity(e);
        REQUIRE_FALSE(world.is_alive(e));
        REQUIRE(world.entity_count() == 0);
    }

    SECTION("Generation increments on reuse") {
        Entity e1 = world.create_entity();
        u32 index = e1.index;
        u32 gen1 = e1.generation;

        world.destroy_entity(e1);
        Entity e2 = world.create_entity();

        REQUIRE(e2.index == index);         // Same slot reused
        REQUIRE(e2.generation == gen1 + 1); // Generation incremented
        REQUIRE_FALSE(world.is_alive(e1));  // Old handle invalid
        REQUIRE(world.is_alive(e2));        // New handle valid
    }

    Log::shutdown();
}

TEST_CASE("World component management", "[ecs][world]") {
    Log::init(LogLevel::Off, LogLevel::Off);
    World world;

    SECTION("Add and get components") {
        Entity e = world.create_entity();

        world.add_component<Position>(e, 1.0f, 2.0f, 3.0f);
        world.add_component<Velocity>(e, 0.1f, 0.2f, 0.3f);

        REQUIRE(world.has_component<Position>(e));
        REQUIRE(world.has_component<Velocity>(e));
        REQUIRE_FALSE(world.has_component<Health>(e));

        REQUIRE(world.get_component<Position>(e)->x == 1.0f);
        REQUIRE(world.get_component<Velocity>(e)->dx == 0.1f);
    }

    SECTION("Remove components") {
        Entity e = world.create_entity();
        world.add_component<Position>(e);
        world.add_component<Velocity>(e);

        REQUIRE(world.has_component<Position>(e));

        world.remove_component<Position>(e);
        REQUIRE_FALSE(world.has_component<Position>(e));
        REQUIRE(world.has_component<Velocity>(e));
    }

    SECTION("Destroying entity removes components") {
        Entity e = world.create_entity();
        world.add_component<Position>(e);
        world.add_component<Velocity>(e);

        world.destroy_entity(e);

        // Component queries should fail gracefully
        REQUIRE_FALSE(world.has_component<Position>(e));
        REQUIRE(world.get_component<Position>(e) == nullptr);
    }

    Log::shutdown();
}
