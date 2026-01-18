/**
 * @file test_asset_handle.cpp
 * @brief Unit tests for AssetHandle template
 */

#include <unordered_set>

#include <catch2/catch_test_macros.hpp>
#include <engine/assets/asset_handle.hpp>

using namespace hz;

// ============================================================================
// AssetHandle Validity Tests
// ============================================================================

namespace {
struct TestAsset {};
struct OtherAsset {};
} // namespace

TEST_CASE("AssetHandle validity", "[assets][handle]") {
    SECTION("Default construction is invalid") {
        AssetHandle<TestAsset> handle;
        REQUIRE_FALSE(handle.is_valid());
        REQUIRE(handle.index == 0);
        REQUIRE(handle.generation == 0);
    }

    SECTION("Zero index and zero generation is invalid") {
        AssetHandle<TestAsset> handle{0, 0};
        REQUIRE_FALSE(handle.is_valid());
    }

    SECTION("Non-zero index with zero generation is valid") {
        AssetHandle<TestAsset> handle{1, 0};
        REQUIRE(handle.is_valid());
    }

    SECTION("Zero index with non-zero generation is valid") {
        AssetHandle<TestAsset> handle{0, 1};
        REQUIRE(handle.is_valid());
    }

    SECTION("Non-zero index and generation is valid") {
        AssetHandle<TestAsset> handle{5, 3};
        REQUIRE(handle.is_valid());
    }

    SECTION("invalid() factory returns invalid handle") {
        auto handle = AssetHandle<TestAsset>::invalid();
        REQUIRE_FALSE(handle.is_valid());
        REQUIRE(handle.index == 0);
        REQUIRE(handle.generation == 0);
    }
}

// ============================================================================
// AssetHandle Equality Tests
// ============================================================================

TEST_CASE("AssetHandle equality operators", "[assets][handle]") {
    SECTION("Same index and generation are equal") {
        AssetHandle<TestAsset> a{10, 5};
        AssetHandle<TestAsset> b{10, 5};

        REQUIRE(a == b);
        REQUIRE_FALSE(a != b);
    }

    SECTION("Same index different generation are not equal") {
        AssetHandle<TestAsset> a{10, 5};
        AssetHandle<TestAsset> b{10, 6};

        REQUIRE_FALSE(a == b);
        REQUIRE(a != b);
    }

    SECTION("Different index same generation are not equal") {
        AssetHandle<TestAsset> a{10, 5};
        AssetHandle<TestAsset> b{11, 5};

        REQUIRE_FALSE(a == b);
        REQUIRE(a != b);
    }

    SECTION("Both invalid handles are equal") {
        AssetHandle<TestAsset> a{0, 0};
        AssetHandle<TestAsset> b = AssetHandle<TestAsset>::invalid();

        REQUIRE(a == b);
    }

    SECTION("Invalid vs valid handles are not equal") {
        AssetHandle<TestAsset> invalid{0, 0};
        AssetHandle<TestAsset> valid{1, 1};

        REQUIRE_FALSE(invalid == valid);
        REQUIRE(invalid != valid);
    }
}

// ============================================================================
// AssetHandle Type Safety Tests
// ============================================================================

TEST_CASE("AssetHandle type safety", "[assets][handle]") {
    // Different asset types should be different types
    AssetHandle<TestAsset> a{10, 5};
    AssetHandle<OtherAsset> b{10, 5};

    // Same numeric values
    REQUIRE(a.index == b.index);
    REQUIRE(a.generation == b.generation);

    // But different types - this verifies they are distinct:
    // Uncommenting the following would cause a compile error (desired):
    // REQUIRE(a == b);  // Error: no match for operator==
}

// ============================================================================
// AssetHandle Hash Tests
// ============================================================================

TEST_CASE("AssetHandle std::hash", "[assets][handle][hash]") {
    std::hash<AssetHandle<TestAsset>> hasher;

    SECTION("Same handles produce same hash") {
        AssetHandle<TestAsset> a{10, 5};
        AssetHandle<TestAsset> b{10, 5};

        REQUIRE(hasher(a) == hasher(b));
    }

    SECTION("Different handles produce different hashes") {
        AssetHandle<TestAsset> a{10, 5};
        AssetHandle<TestAsset> b{10, 6};
        AssetHandle<TestAsset> c{11, 5};

        // Different generation
        REQUIRE(hasher(a) != hasher(b));
        // Different index
        REQUIRE(hasher(a) != hasher(c));
    }

    SECTION("Can use in unordered_set") {
        std::unordered_set<AssetHandle<TestAsset>> set;

        AssetHandle<TestAsset> h1{1, 1};
        AssetHandle<TestAsset> h2{2, 1};
        AssetHandle<TestAsset> h1_copy{1, 1};

        set.insert(h1);
        set.insert(h2);
        set.insert(h1_copy); // Duplicate

        REQUIRE(set.size() == 2);
        REQUIRE(set.count(h1) == 1);
        REQUIRE(set.count(h2) == 1);
    }

    SECTION("Can use in unordered_map") {
        std::unordered_map<AssetHandle<TestAsset>, std::string> map;

        AssetHandle<TestAsset> h1{1, 1};
        AssetHandle<TestAsset> h2{2, 1};

        map[h1] = "first";
        map[h2] = "second";

        REQUIRE(map.at(h1) == "first");
        REQUIRE(map.at(h2) == "second");

        // Update existing
        map[h1] = "updated";
        REQUIRE(map.at(h1) == "updated");
        REQUIRE(map.size() == 2);
    }
}

// ============================================================================
// Common Handle Types Tests
// ============================================================================

TEST_CASE("Common asset handle types", "[assets][handle]") {
    // Verify the type aliases work correctly
    SECTION("TextureHandle") {
        TextureHandle handle{1, 1};
        REQUIRE(handle.is_valid());
        REQUIRE(handle.index == 1);
        REQUIRE(handle.generation == 1);
    }

    SECTION("ModelHandle") {
        ModelHandle handle{2, 3};
        REQUIRE(handle.is_valid());
        REQUIRE(handle.index == 2);
        REQUIRE(handle.generation == 3);
    }

    SECTION("MaterialHandle") {
        MaterialHandle handle{4, 5};
        REQUIRE(handle.is_valid());
        REQUIRE(handle.index == 4);
        REQUIRE(handle.generation == 5);
    }

    SECTION("Different handle types are distinct") {
        // All have same values but different types
        TextureHandle tex{1, 1};
        ModelHandle model{1, 1};
        MaterialHandle mat{1, 1};

        // Same underlying values
        REQUIRE(tex.index == model.index);
        REQUIRE(model.index == mat.index);
        REQUIRE(tex.generation == model.generation);

        // These would cause compile errors if uncommented (desired):
        // REQUIRE(tex == model);
        // REQUIRE(model == mat);
    }
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_CASE("AssetHandle edge cases", "[assets][handle]") {
    SECTION("Maximum index value") {
        AssetHandle<TestAsset> handle{std::numeric_limits<u32>::max(), 1};
        REQUIRE(handle.is_valid());
        REQUIRE(handle.index == std::numeric_limits<u32>::max());
    }

    SECTION("Maximum generation value") {
        AssetHandle<TestAsset> handle{1, std::numeric_limits<u32>::max()};
        REQUIRE(handle.is_valid());
        REQUIRE(handle.generation == std::numeric_limits<u32>::max());
    }

    SECTION("Hash uniqueness for adjacent values") {
        std::hash<AssetHandle<TestAsset>> hasher;

        // Test that adjacent index/generation values produce different hashes
        AssetHandle<TestAsset> h1{0, 1};
        AssetHandle<TestAsset> h2{1, 0};

        // These could theoretically collide but shouldn't with good hash
        // The hash combines index (shifted) with generation
        REQUIRE(hasher(h1) != hasher(h2));
    }
}
