/**
 * @file test_memory.cpp
 * @brief Unit tests for the memory management system
 */

#include <catch2/catch_test_macros.hpp>
#include <engine/core/memory.hpp>

using namespace hz;

// ============================================================================
// LinearArena Tests
// ============================================================================

TEST_CASE("LinearArena basic operations", "[memory][arena]") {
    LinearArena arena(1024);

    SECTION("Initial state") {
        REQUIRE(arena.capacity() == 1024);
        REQUIRE(arena.used() == 0);
        REQUIRE(arena.usage_percent() == 0.0f);
    }

    SECTION("Allocate memory") {
        void* ptr1 = arena.allocate(100, 8);
        REQUIRE(ptr1 != nullptr);
        REQUIRE(arena.used() >= 100);

        void* ptr2 = arena.allocate(200, 8);
        REQUIRE(ptr2 != nullptr);
        REQUIRE(ptr2 != ptr1);
        REQUIRE(arena.used() >= 300);
    }

    SECTION("Reset clears allocations") {
        (void)arena.allocate(500, 8);
        REQUIRE(arena.used() > 0);

        arena.reset();
        REQUIRE(arena.used() == 0);
    }

    SECTION("Alignment is respected") {
        // Force misalignment
        (void)arena.allocate(3, 1);

        // Allocate with 16-byte alignment
        void* ptr = arena.allocate(32, 16);
        REQUIRE(reinterpret_cast<uintptr_t>(ptr) % 16 == 0);
    }

    SECTION("ScopedArenaMarker restores offset on destruction") {
        // Allocate some initial data
        (void)arena.allocate(100, 8);
        auto used_before = arena.used();
        REQUIRE(used_before >= 100);

        {
            ScopedArenaMarker marker(arena);
            // Allocate more inside the scoped block
            (void)arena.allocate(200, 8);
            (void)arena.allocate(300, 8);
            REQUIRE(arena.used() > used_before);
        }
        // After marker destruction, offset should be restored
        REQUIRE(arena.used() == used_before);
    }

    SECTION("ScopedArenaMarker restores to zero when arena was empty") {
        REQUIRE(arena.used() == 0);

        {
            ScopedArenaMarker marker(arena);
            (void)arena.allocate(256, 8);
            REQUIRE(arena.used() >= 256);
        }
        REQUIRE(arena.used() == 0);
    }
}

// ============================================================================
// PMR Vector Tests
// ============================================================================

TEST_CASE("PMR vector with arena", "[memory][pmr]") {
    LinearArena arena(4096);

    SECTION("Vector uses arena") {
        std::pmr::vector<int> vec(&arena);

        for (int i = 0; i < 100; ++i) {
            vec.push_back(i);
        }

        REQUIRE(vec.size() == 100);
        REQUIRE(arena.used() > 0);

        // Verify data
        for (int i = 0; i < 100; ++i) {
            REQUIRE(vec[static_cast<size_t>(i)] == i);
        }
    }

    SECTION("Multiple vectors share arena") {
        std::pmr::vector<int> vec1(&arena);
        std::pmr::vector<float> vec2(&arena);

        vec1.resize(10, 42);
        vec2.resize(10, 3.14f);

        REQUIRE(vec1.size() == 10);
        REQUIRE(vec2.size() == 10);
    }
}
