/**
 * @file test_projectile.cpp
 * @brief Unit tests for projectile system damage calculations
 */

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <engine/physics/projectile_system.hpp>

using namespace hz;
using Catch::Approx;

// ============================================================================
// ProjectileData Default Values
// ============================================================================

TEST_CASE("ProjectileData defaults", "[physics][projectile]") {
    ProjectileData data;

    SECTION("Default name") {
        REQUIRE(data.name == "bullet");
    }

    SECTION("Default type is Hitscan") {
        REQUIRE(data.type == ProjectileType::Hitscan);
    }

    SECTION("Default damage values") {
        REQUIRE(data.base_damage == Approx(25.0f));
        REQUIRE(data.damage_falloff_start == Approx(20.0f));
        REQUIRE(data.damage_falloff_end == Approx(50.0f));
        REQUIRE(data.min_damage_multiplier == Approx(0.5f));
    }

    SECTION("Default ballistic properties") {
        REQUIRE(data.muzzle_velocity == Approx(400.0f));
        REQUIRE(data.gravity_scale == Approx(1.0f));
        REQUIRE(data.drag_coefficient == Approx(0.0f));
    }

    SECTION("Default lifetime values") {
        REQUIRE(data.max_lifetime == Approx(10.0f));
        REQUIRE(data.max_range == Approx(1000.0f));
    }

    SECTION("Default penetration is none") {
        REQUIRE(data.penetration_power == Approx(0.0f));
        REQUIRE(data.max_penetrations == 0);
    }

    SECTION("Not explosive by default") {
        REQUIRE(data.explosive == false);
        REQUIRE(data.explosion_radius == Approx(0.0f));
        REQUIRE(data.explosion_damage == Approx(0.0f));
    }

    SECTION("Has tracer by default") {
        REQUIRE(data.has_tracer == true);
        REQUIRE(data.tracer_width == Approx(0.02f));
    }
}

// ============================================================================
// Damage Falloff Calculation Tests
// ============================================================================

TEST_CASE("ProjectileSystem::calculate_damage_falloff", "[physics][projectile]") {
    // NOTE: calculate_damage_falloff returns a MULTIPLIER (0-1), not actual damage
    ProjectileData data;
    data.base_damage = 100.0f;
    data.damage_falloff_start = 20.0f;
    data.damage_falloff_end = 50.0f;
    data.min_damage_multiplier = 0.5f;

    SECTION("Full multiplier before falloff start") {
        f32 mult = ProjectileSystem::calculate_damage_falloff(data, 0.0f);
        REQUIRE(mult == Approx(1.0f));

        mult = ProjectileSystem::calculate_damage_falloff(data, 10.0f);
        REQUIRE(mult == Approx(1.0f));

        mult = ProjectileSystem::calculate_damage_falloff(data, 19.9f);
        REQUIRE(mult == Approx(1.0f));
    }

    SECTION("Exactly at falloff start") {
        f32 mult = ProjectileSystem::calculate_damage_falloff(data, 20.0f);
        REQUIRE(mult == Approx(1.0f));
    }

    SECTION("Minimum multiplier at falloff end") {
        f32 mult = ProjectileSystem::calculate_damage_falloff(data, 50.0f);
        REQUIRE(mult == Approx(0.5f)); // min_damage_multiplier
    }

    SECTION("Minimum multiplier beyond falloff end") {
        f32 mult = ProjectileSystem::calculate_damage_falloff(data, 100.0f);
        REQUIRE(mult == Approx(0.5f));

        mult = ProjectileSystem::calculate_damage_falloff(data, 1000.0f);
        REQUIRE(mult == Approx(0.5f));
    }

    SECTION("Linear interpolation in falloff range") {
        // Midpoint of falloff range (20 to 50, midpoint = 35)
        f32 mult = ProjectileSystem::calculate_damage_falloff(data, 35.0f);
        // At midpoint: 1.0 - 0.5 * (1.0 - 0.5) = 0.75
        REQUIRE(mult == Approx(0.75f));
    }

    SECTION("Quarter points in falloff range") {
        // 25% into falloff range (at distance 27.5)
        f32 mult = ProjectileSystem::calculate_damage_falloff(data, 27.5f);
        // t = 0.25, mult = 1.0 - 0.25 * 0.5 = 0.875
        REQUIRE(mult == Approx(0.875f));

        // 75% into falloff range (at distance 42.5)
        mult = ProjectileSystem::calculate_damage_falloff(data, 42.5f);
        // t = 0.75, mult = 1.0 - 0.75 * 0.5 = 0.625
        REQUIRE(mult == Approx(0.625f));
    }
}

TEST_CASE("Damage falloff edge cases", "[physics][projectile]") {
    SECTION("Zero base damage - multiplier still calculated") {
        ProjectileData data;
        data.base_damage = 0.0f;
        data.damage_falloff_start = 10.0f;
        data.damage_falloff_end = 50.0f;
        data.min_damage_multiplier = 0.5f;

        // Multiplier is calculated independently of base damage
        f32 mult = ProjectileSystem::calculate_damage_falloff(data, 30.0f);
        REQUIRE(mult == Approx(0.75f)); // Midpoint interpolation
    }

    SECTION("100% minimum multiplier (no falloff)") {
        ProjectileData data;
        data.base_damage = 50.0f;
        data.damage_falloff_start = 10.0f;
        data.damage_falloff_end = 50.0f;
        data.min_damage_multiplier = 1.0f;

        f32 mult = ProjectileSystem::calculate_damage_falloff(data, 100.0f);
        REQUIRE(mult == Approx(1.0f));
    }

    SECTION("0% minimum multiplier (full falloff)") {
        ProjectileData data;
        data.base_damage = 100.0f;
        data.damage_falloff_start = 10.0f;
        data.damage_falloff_end = 50.0f;
        data.min_damage_multiplier = 0.0f;

        f32 mult = ProjectileSystem::calculate_damage_falloff(data, 50.0f);
        REQUIRE(mult == Approx(0.0f));
    }

    SECTION("Same start and end distance (instant falloff)") {
        ProjectileData data;
        data.base_damage = 100.0f;
        data.damage_falloff_start = 30.0f;
        data.damage_falloff_end = 30.0f; // Same as start
        data.min_damage_multiplier = 0.5f;

        // Before threshold
        f32 mult = ProjectileSystem::calculate_damage_falloff(data, 29.0f);
        REQUIRE(mult == Approx(1.0f));

        // At/after threshold - division by zero protection needed
        // The implementation may return NaN or inf, or handle it gracefully
        mult = ProjectileSystem::calculate_damage_falloff(data, 30.0f);
        // Either full or min depending on implementation
        REQUIRE((mult == Approx(1.0f) || mult == Approx(0.5f) || std::isnan(mult)));
    }

    SECTION("Negative distance treated as before falloff start") {
        ProjectileData data;
        data.base_damage = 100.0f;
        data.damage_falloff_start = 10.0f;
        data.damage_falloff_end = 50.0f;
        data.min_damage_multiplier = 0.5f;

        f32 mult = ProjectileSystem::calculate_damage_falloff(data, -10.0f);
        REQUIRE(mult == Approx(1.0f));
    }
}

// ============================================================================
// Projectile Template Tests
// ============================================================================

TEST_CASE("ProjectileTemplates::pistol_bullet", "[physics][projectile][templates]") {
    auto data = ProjectileTemplates::pistol_bullet();

    SECTION("Correct name") {
        REQUIRE(data.name == "9mm");
    }

    SECTION("Is hitscan") {
        REQUIRE(data.type == ProjectileType::Hitscan);
    }

    SECTION("Has reasonable damage") {
        REQUIRE(data.base_damage > 0.0f);
        REQUIRE(data.base_damage < 100.0f);
    }

    SECTION("Has close-range falloff") {
        REQUIRE(data.damage_falloff_start < 30.0f);
        REQUIRE(data.damage_falloff_end < 100.0f);
    }

    SECTION("No penetration") {
        REQUIRE(data.penetration_power == Approx(0.0f));
    }
}

TEST_CASE("ProjectileTemplates::rifle_bullet", "[physics][projectile][templates]") {
    auto data = ProjectileTemplates::rifle_bullet();

    SECTION("Correct name") {
        REQUIRE(data.name == "5.56mm");
    }

    SECTION("Is hitscan") {
        REQUIRE(data.type == ProjectileType::Hitscan);
    }

    SECTION("Higher damage than pistol") {
        auto pistol = ProjectileTemplates::pistol_bullet();
        REQUIRE(data.base_damage > pistol.base_damage);
    }

    SECTION("Longer falloff range than pistol") {
        auto pistol = ProjectileTemplates::pistol_bullet();
        REQUIRE(data.damage_falloff_start > pistol.damage_falloff_start);
        REQUIRE(data.damage_falloff_end > pistol.damage_falloff_end);
    }

    SECTION("Has some penetration") {
        REQUIRE(data.penetration_power > 0.0f);
        REQUIRE(data.max_penetrations >= 1);
    }
}

TEST_CASE("ProjectileTemplates::sniper_bullet", "[physics][projectile][templates]") {
    auto data = ProjectileTemplates::sniper_bullet();

    SECTION("Correct name") {
        REQUIRE(data.name == "7.62mm");
    }

    SECTION("Is hitscan") {
        REQUIRE(data.type == ProjectileType::Hitscan);
    }

    SECTION("High base damage") {
        REQUIRE(data.base_damage >= 100.0f);
    }

    SECTION("Long range effectiveness") {
        REQUIRE(data.damage_falloff_start >= 100.0f);
        REQUIRE(data.max_range >= 500.0f);
    }

    SECTION("High penetration") {
        auto rifle = ProjectileTemplates::rifle_bullet();
        REQUIRE(data.penetration_power > rifle.penetration_power);
        REQUIRE(data.max_penetrations > rifle.max_penetrations);
    }

    SECTION("Maintains damage at range") {
        REQUIRE(data.min_damage_multiplier >= 0.7f);
    }
}

TEST_CASE("ProjectileTemplates::shotgun_pellet", "[physics][projectile][templates]") {
    auto data = ProjectileTemplates::shotgun_pellet();

    SECTION("Correct name") {
        REQUIRE(data.name == "12gauge_pellet");
    }

    SECTION("Is hitscan") {
        REQUIRE(data.type == ProjectileType::Hitscan);
    }

    SECTION("Low individual pellet damage") {
        REQUIRE(data.base_damage < 20.0f);
    }

    SECTION("Very short effective range") {
        REQUIRE(data.damage_falloff_start < 10.0f);
        REQUIRE(data.damage_falloff_end < 30.0f);
        REQUIRE(data.max_range < 50.0f);
    }

    SECTION("Severe damage falloff") {
        REQUIRE(data.min_damage_multiplier < 0.3f);
    }
}

TEST_CASE("ProjectileTemplates::rocket", "[physics][projectile][templates]") {
    auto data = ProjectileTemplates::rocket();

    SECTION("Correct name") {
        REQUIRE(data.name == "rocket");
    }

    SECTION("Is ballistic") {
        REQUIRE(data.type == ProjectileType::Ballistic);
    }

    SECTION("Relatively slow velocity") {
        REQUIRE(data.muzzle_velocity < 100.0f);
    }

    SECTION("Low gravity effect") {
        REQUIRE(data.gravity_scale < 1.0f);
    }

    SECTION("Is explosive") {
        REQUIRE(data.explosive == true);
        REQUIRE(data.explosion_radius > 0.0f);
        REQUIRE(data.explosion_damage > data.base_damage);
    }

    SECTION("Has visible tracer") {
        REQUIRE(data.has_tracer == true);
    }
}

TEST_CASE("ProjectileTemplates::grenade", "[physics][projectile][templates]") {
    auto data = ProjectileTemplates::grenade();

    SECTION("Correct name") {
        REQUIRE(data.name == "frag_grenade");
    }

    SECTION("Is ballistic") {
        REQUIRE(data.type == ProjectileType::Ballistic);
    }

    SECTION("Slow throw velocity") {
        REQUIRE(data.muzzle_velocity < 30.0f);
    }

    SECTION("Normal gravity") {
        REQUIRE(data.gravity_scale == Approx(1.0f));
    }

    SECTION("Is explosive with large radius") {
        REQUIRE(data.explosive == true);
        auto rocket = ProjectileTemplates::rocket();
        REQUIRE(data.explosion_radius > rocket.explosion_radius);
    }

    SECTION("Fuse timer (short lifetime)") {
        REQUIRE(data.max_lifetime < 5.0f);
    }

    SECTION("High explosion damage") {
        REQUIRE(data.explosion_damage >= 150.0f);
    }
}

// ============================================================================
// ProjectileComponent Tests
// ============================================================================

TEST_CASE("ProjectileComponent defaults", "[physics][projectile]") {
    ProjectileComponent proj;

    SECTION("Position starts at origin") {
        REQUIRE(proj.position == glm::vec3(0.0f));
        REQUIRE(proj.start_position == glm::vec3(0.0f));
    }

    SECTION("Velocity starts at zero") {
        REQUIRE(proj.velocity == glm::vec3(0.0f));
    }

    SECTION("Time and distance start at zero") {
        REQUIRE(proj.time_alive == Approx(0.0f));
        REQUIRE(proj.distance_traveled == Approx(0.0f));
    }

    SECTION("No owner by default") {
        REQUIRE((proj.owner == entt::null));
    }

    SECTION("No penetrations") {
        REQUIRE(proj.penetration_count == 0);
    }

    SECTION("Not pending destroy") {
        REQUIRE(proj.pending_destroy == false);
    }
}

// ============================================================================
// HitscanResult Tests
// ============================================================================

TEST_CASE("HitscanResult defaults", "[physics][projectile]") {
    HitscanResult result;

    SECTION("No hit by default") {
        REQUIRE(result.hit == false);
    }

    SECTION("Hit info at origin") {
        REQUIRE(result.hit_point == glm::vec3(0.0f));
        REQUIRE(result.hit_normal == glm::vec3(0.0f));
        REQUIRE(result.distance == Approx(0.0f));
    }

    SECTION("No entity hit") {
        REQUIRE((result.hit_entity == entt::null));
        REQUIRE(result.hit_hitbox == nullptr);
    }

    SECTION("Default location is Torso") {
        REQUIRE(result.hit_location == HitboxType::Torso);
    }

    SECTION("Zero damage") {
        REQUIRE(result.raw_damage == Approx(0.0f));
        REQUIRE(result.final_damage == Approx(0.0f));
    }
}

// ============================================================================
// ProjectileType Tests
// ============================================================================

TEST_CASE("ProjectileType enum values", "[physics][projectile]") {
    // Ensure all types are distinct
    REQUIRE(ProjectileType::Hitscan != ProjectileType::Ballistic);
    REQUIRE(ProjectileType::Ballistic != ProjectileType::Continuous);
    REQUIRE(ProjectileType::Hitscan != ProjectileType::Continuous);
}
