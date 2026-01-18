/**
 * @file test_types.cpp
 * @brief Unit tests for core type definitions
 */

#include <unordered_set>

#include <catch2/catch_test_macros.hpp>
#include <engine/core/types.hpp>

using namespace hz;

// ============================================================================
// Handle<Tag, T> Tests
// ============================================================================

namespace {
struct TestTag {};
struct OtherTag {};
} // namespace

TEST_CASE("Handle basic operations", "[types][handle]") {
    SECTION("Default construction is invalid") {
        Handle<TestTag> handle;
        REQUIRE_FALSE(handle.is_valid());
        REQUIRE_FALSE(static_cast<bool>(handle));
        REQUIRE(handle.value == Handle<TestTag>::INVALID_VALUE);
    }

    SECTION("Explicit construction with valid value") {
        Handle<TestTag> handle(42);
        REQUIRE(handle.is_valid());
        REQUIRE(static_cast<bool>(handle));
        REQUIRE(handle.value == 42);
    }

    SECTION("Construction with zero is valid") {
        Handle<TestTag> handle(0);
        REQUIRE(handle.is_valid());
        REQUIRE(handle.value == 0);
    }

    SECTION("Construction with max-1 is valid") {
        Handle<TestTag> handle(Handle<TestTag>::INVALID_VALUE - 1);
        REQUIRE(handle.is_valid());
    }

    SECTION("Explicit construction with INVALID_VALUE is invalid") {
        Handle<TestTag> handle(Handle<TestTag>::INVALID_VALUE);
        REQUIRE_FALSE(handle.is_valid());
    }
}

TEST_CASE("Handle comparison operators", "[types][handle]") {
    SECTION("Equality") {
        Handle<TestTag> a(10);
        Handle<TestTag> b(10);
        Handle<TestTag> c(20);

        REQUIRE(a == b);
        REQUIRE_FALSE(a == c);
    }

    SECTION("Inequality") {
        Handle<TestTag> a(10);
        Handle<TestTag> b(20);

        REQUIRE(a != b);
    }

    SECTION("Ordering") {
        Handle<TestTag> a(10);
        Handle<TestTag> b(20);

        REQUIRE(a < b);
        REQUIRE(b > a);
        REQUIRE(a <= b);
        REQUIRE(b >= a);
        REQUIRE(a <= a);
        REQUIRE(a >= a);
    }

    SECTION("Invalid handles compare equal") {
        Handle<TestTag> a;
        Handle<TestTag> b;

        REQUIRE(a == b);
    }
}

TEST_CASE("Handle type safety", "[types][handle]") {
    // These should be different types at compile time
    // This test mainly verifies the code compiles correctly
    Handle<TestTag> a(10);
    Handle<OtherTag> b(10);

    // Both have same numeric value but are different types
    REQUIRE(a.value == b.value);

    // Uncommenting below would cause a compile error (desired behavior):
    // REQUIRE(a == b);  // Error: no match for operator==
}

TEST_CASE("Handle with different underlying types", "[types][handle]") {
    SECTION("u64 handle") {
        Handle<TestTag, u64> handle(std::numeric_limits<u64>::max() - 1);
        REQUIRE(handle.is_valid());
        REQUIRE(handle.value == std::numeric_limits<u64>::max() - 1);
    }

    SECTION("u16 handle") {
        Handle<TestTag, u16> handle(1000);
        REQUIRE(handle.is_valid());
        REQUIRE(Handle<TestTag, u16>::INVALID_VALUE == std::numeric_limits<u16>::max());
    }
}

// ============================================================================
// GenerationalHandle<Tag> Tests
// ============================================================================

TEST_CASE("GenerationalHandle basic operations", "[types][generational]") {
    SECTION("Default construction is invalid") {
        GenerationalHandle<TestTag> handle;
        REQUIRE_FALSE(handle.is_valid());
        REQUIRE_FALSE(static_cast<bool>(handle));
        REQUIRE(handle.index == GenerationalHandle<TestTag>::INVALID_INDEX);
        REQUIRE(handle.generation == GenerationalHandle<TestTag>::INVALID_GENERATION);
    }

    SECTION("Construction with valid values") {
        GenerationalHandle<TestTag> handle(5, 1);
        REQUIRE(handle.is_valid());
        REQUIRE(static_cast<bool>(handle));
        REQUIRE(handle.index == 5);
        REQUIRE(handle.generation == 1);
    }

    SECTION("Index 0 with valid generation is valid") {
        GenerationalHandle<TestTag> handle(0, 1);
        REQUIRE(handle.is_valid());
    }

    SECTION("Valid index with generation 0 is invalid") {
        GenerationalHandle<TestTag> handle(5, 0);
        REQUIRE_FALSE(handle.is_valid());
    }

    SECTION("INVALID_INDEX with any generation is invalid") {
        GenerationalHandle<TestTag> handle(GenerationalHandle<TestTag>::INVALID_INDEX, 100);
        REQUIRE_FALSE(handle.is_valid());
    }
}

TEST_CASE("GenerationalHandle equality", "[types][generational]") {
    SECTION("Same index and generation are equal") {
        GenerationalHandle<TestTag> a(10, 5);
        GenerationalHandle<TestTag> b(10, 5);
        REQUIRE(a == b);
    }

    SECTION("Same index different generation are not equal") {
        GenerationalHandle<TestTag> a(10, 5);
        GenerationalHandle<TestTag> b(10, 6);
        REQUIRE_FALSE(a == b);
    }

    SECTION("Different index same generation are not equal") {
        GenerationalHandle<TestTag> a(10, 5);
        GenerationalHandle<TestTag> b(11, 5);
        REQUIRE_FALSE(a == b);
    }

    SECTION("Invalid handles are equal") {
        GenerationalHandle<TestTag> a;
        GenerationalHandle<TestTag> b;
        REQUIRE(a == b);
    }
}

// ============================================================================
// TransparentStringHash Tests
// ============================================================================

TEST_CASE("TransparentStringHash operations", "[types][hash]") {
    TransparentStringHash hasher;

    SECTION("Same strings produce same hash") {
        std::string str = "test";
        std::string_view sv = "test";
        const char* cstr = "test";

        size_t hash1 = hasher(str);
        size_t hash2 = hasher(sv);
        size_t hash3 = hasher(cstr);

        REQUIRE(hash1 == hash2);
        REQUIRE(hash2 == hash3);
    }

    SECTION("Different strings produce different hashes") {
        size_t hash1 = hasher("hello");
        size_t hash2 = hasher("world");

        REQUIRE(hash1 != hash2);
    }

    SECTION("Empty string has consistent hash") {
        std::string empty_str;
        std::string_view empty_sv;
        const char* empty_cstr = "";

        REQUIRE(hasher(empty_str) == hasher(empty_sv));
        REQUIRE(hasher(empty_sv) == hasher(empty_cstr));
    }

    SECTION("Can be used with unordered_map for heterogeneous lookup") {
        std::unordered_map<std::string, int, TransparentStringHash, std::equal_to<>> map;
        map["key1"] = 100;
        map["key2"] = 200;

        // Lookup with string_view (no allocation)
        std::string_view sv = "key1";
        auto it = map.find(sv);
        REQUIRE(it != map.end());
        REQUIRE(it->second == 100);

        // Lookup with const char*
        it = map.find("key2");
        REQUIRE(it != map.end());
        REQUIRE(it->second == 200);
    }
}

// ============================================================================
// Constants Tests
// ============================================================================

TEST_CASE("Mathematical constants", "[types][constants]") {
    SECTION("PI is approximately correct") {
        REQUIRE(PI > 3.14159);
        REQUIRE(PI < 3.14160);
    }

    SECTION("TAU is 2*PI") {
        REQUIRE(TAU == 2.0 * PI);
    }

    SECTION("EPSILON is small positive") {
        REQUIRE(EPSILON > 0.0);
        REQUIRE(EPSILON < 0.001);
    }
}

// ============================================================================
// Type Size Verification
// ============================================================================

TEST_CASE("Type sizes are correct", "[types][size]") {
    REQUIRE(sizeof(u8) == 1);
    REQUIRE(sizeof(u16) == 2);
    REQUIRE(sizeof(u32) == 4);
    REQUIRE(sizeof(u64) == 8);

    REQUIRE(sizeof(i8) == 1);
    REQUIRE(sizeof(i16) == 2);
    REQUIRE(sizeof(i32) == 4);
    REQUIRE(sizeof(i64) == 8);

    REQUIRE(sizeof(f32) == 4);
    REQUIRE(sizeof(f64) == 8);
}
