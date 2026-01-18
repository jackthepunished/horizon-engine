/**
 * @file test_hitbox_system.cpp
 * @brief Unit tests for hitbox/hurtbox damage system
 */

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <engine/physics/hitbox_system.hpp>

using namespace hz;
using Catch::Approx;

// ============================================================================
// Damage Multiplier Tests
// ============================================================================

TEST_CASE("get_default_damage_multiplier", "[physics][hitbox]") {
    SECTION("Head has 2x multiplier (headshot)") {
        REQUIRE(get_default_damage_multiplier(HitboxType::Head) == Approx(2.0f));
    }

    SECTION("Torso has 1x multiplier") {
        REQUIRE(get_default_damage_multiplier(HitboxType::Torso) == Approx(1.0f));
    }

    SECTION("Arms have 0.75x multiplier") {
        REQUIRE(get_default_damage_multiplier(HitboxType::LeftArm) == Approx(0.75f));
        REQUIRE(get_default_damage_multiplier(HitboxType::RightArm) == Approx(0.75f));
    }

    SECTION("Legs have 0.75x multiplier") {
        REQUIRE(get_default_damage_multiplier(HitboxType::LeftLeg) == Approx(0.75f));
        REQUIRE(get_default_damage_multiplier(HitboxType::RightLeg) == Approx(0.75f));
    }

    SECTION("Custom type defaults to 1x") {
        REQUIRE(get_default_damage_multiplier(HitboxType::Custom) == Approx(1.0f));
    }
}

// ============================================================================
// Hitbox Struct Tests
// ============================================================================

TEST_CASE("Hitbox default values", "[physics][hitbox]") {
    Hitbox hitbox;

    SECTION("Has default name") {
        REQUIRE(hitbox.name == "hitbox");
    }

    SECTION("Default type is Torso") {
        REQUIRE(hitbox.type == HitboxType::Torso);
    }

    SECTION("Default shape is Capsule") {
        REQUIRE(hitbox.shape == HitboxShape::Capsule);
    }

    SECTION("Default offset is zero") {
        REQUIRE(hitbox.offset == glm::vec3(0.0f));
    }

    SECTION("Default rotation is zero") {
        REQUIRE(hitbox.rotation == glm::vec3(0.0f));
    }

    SECTION("Default damage multiplier is 1.0") {
        REQUIRE(hitbox.damage_multiplier == Approx(1.0f));
    }

    SECTION("Is enabled by default") {
        REQUIRE(hitbox.enabled == true);
    }
}

// ============================================================================
// HurtboxComponent Tests
// ============================================================================

TEST_CASE("HurtboxComponent default values", "[physics][hurtbox]") {
    HurtboxComponent hurtbox;

    SECTION("Default health values") {
        REQUIRE(hurtbox.max_health == Approx(100.0f));
        REQUIRE(hurtbox.current_health == Approx(100.0f));
    }

    SECTION("Default armor values") {
        REQUIRE(hurtbox.armor == Approx(0.0f));
        REQUIRE(hurtbox.max_armor == Approx(100.0f));
        REQUIRE(hurtbox.armor_effectiveness == Approx(0.5f));
    }

    SECTION("Not invulnerable by default") {
        REQUIRE(hurtbox.invulnerable == false);
        REQUIRE(hurtbox.invulnerability_timer == Approx(0.0f));
    }

    SECTION("Not dead by default") {
        REQUIRE(hurtbox.is_dead == false);
    }
}

TEST_CASE("HurtboxComponent::heal", "[physics][hurtbox]") {
    HurtboxComponent hurtbox;
    hurtbox.max_health = 100.0f;
    hurtbox.current_health = 50.0f;

    SECTION("Heal partial amount") {
        hurtbox.heal(25.0f);
        REQUIRE(hurtbox.current_health == Approx(75.0f));
    }

    SECTION("Heal to max health") {
        hurtbox.heal(50.0f);
        REQUIRE(hurtbox.current_health == Approx(100.0f));
    }

    SECTION("Over-heal is clamped to max") {
        hurtbox.heal(100.0f);
        REQUIRE(hurtbox.current_health == Approx(100.0f));
    }

    SECTION("Heal zero does nothing") {
        hurtbox.heal(0.0f);
        REQUIRE(hurtbox.current_health == Approx(50.0f));
    }

    SECTION("Heal from nearly dead") {
        hurtbox.current_health = 1.0f;
        hurtbox.heal(10.0f);
        REQUIRE(hurtbox.current_health == Approx(11.0f));
    }
}

TEST_CASE("HurtboxComponent::add_armor", "[physics][hurtbox]") {
    HurtboxComponent hurtbox;
    hurtbox.max_armor = 100.0f;
    hurtbox.armor = 0.0f;

    SECTION("Add armor from zero") {
        hurtbox.add_armor(50.0f);
        REQUIRE(hurtbox.armor == Approx(50.0f));
    }

    SECTION("Add armor to existing") {
        hurtbox.armor = 30.0f;
        hurtbox.add_armor(25.0f);
        REQUIRE(hurtbox.armor == Approx(55.0f));
    }

    SECTION("Armor capped at max") {
        hurtbox.armor = 80.0f;
        hurtbox.add_armor(50.0f);
        REQUIRE(hurtbox.armor == Approx(100.0f));
    }

    SECTION("Add zero armor does nothing") {
        hurtbox.armor = 50.0f;
        hurtbox.add_armor(0.0f);
        REQUIRE(hurtbox.armor == Approx(50.0f));
    }
}

TEST_CASE("HurtboxComponent::apply_damage basic", "[physics][hurtbox]") {
    HurtboxComponent hurtbox;
    hurtbox.max_health = 100.0f;
    hurtbox.current_health = 100.0f;
    hurtbox.armor = 0.0f;

    SECTION("Basic damage with no armor") {
        f32 dealt = hurtbox.apply_damage(25.0f, HitboxType::Torso, glm::vec3(1, 0, 0));
        REQUIRE(dealt == Approx(25.0f));
        REQUIRE(hurtbox.current_health == Approx(75.0f));
    }

    SECTION("Damage updates last hit info") {
        glm::vec3 direction(1.0f, 0.0f, 0.0f);
        // Note: HitboxType::Head applies default 2x multiplier
        // So 30 base damage becomes 60 actual damage
        f32 dealt = hurtbox.apply_damage(30.0f, HitboxType::Head, direction);

        REQUIRE(hurtbox.last_damage_amount == Approx(dealt));
        REQUIRE(hurtbox.last_hit_location == HitboxType::Head);
        REQUIRE(hurtbox.last_damage_direction == direction);
    }

    SECTION("Damage that kills sets is_dead") {
        hurtbox.current_health = 20.0f;
        hurtbox.apply_damage(50.0f, HitboxType::Torso, glm::vec3(0));

        REQUIRE(hurtbox.current_health <= 0.0f);
        REQUIRE(hurtbox.is_dead == true);
    }

    SECTION("Health clamps at zero") {
        hurtbox.current_health = 10.0f;
        hurtbox.apply_damage(100.0f, HitboxType::Torso, glm::vec3(0));

        REQUIRE(hurtbox.current_health == Approx(0.0f));
    }

    SECTION("Invulnerable takes no damage") {
        hurtbox.invulnerable = true;
        f32 dealt = hurtbox.apply_damage(50.0f, HitboxType::Torso, glm::vec3(0));

        REQUIRE(dealt == Approx(0.0f));
        REQUIRE(hurtbox.current_health == Approx(100.0f));
    }

    SECTION("Dead entity takes no damage") {
        hurtbox.is_dead = true;
        hurtbox.current_health = 0.0f;
        f32 dealt = hurtbox.apply_damage(50.0f, HitboxType::Torso, glm::vec3(0));

        REQUIRE(dealt == Approx(0.0f));
    }
}

TEST_CASE("HurtboxComponent::apply_damage with armor", "[physics][hurtbox]") {
    HurtboxComponent hurtbox;
    hurtbox.max_health = 100.0f;
    hurtbox.current_health = 100.0f;
    hurtbox.max_armor = 100.0f;
    hurtbox.armor = 50.0f;
    hurtbox.armor_effectiveness = 0.5f; // 50% damage reduction from armor

    SECTION("Armor reduces damage") {
        // With 50 armor and 0.5 effectiveness:
        // Armor absorbs: 50% of damage (effectiveness)
        // So 25 damage with armor: 12.5 absorbed by armor, 12.5 to health
        f32 dealt = hurtbox.apply_damage(25.0f, HitboxType::Torso, glm::vec3(0));

        // Some damage went to armor, some to health
        REQUIRE(hurtbox.armor < 50.0f);
        REQUIRE(hurtbox.current_health < 100.0f);
        REQUIRE(dealt > 0.0f);
    }

    SECTION("Armor depletes before health") {
        hurtbox.armor = 10.0f;
        hurtbox.apply_damage(100.0f, HitboxType::Torso, glm::vec3(0));

        // Armor should be depleted
        REQUIRE(hurtbox.armor == Approx(0.0f));
        // Health should have taken remaining damage
        REQUIRE(hurtbox.current_health < 100.0f);
    }
}

TEST_CASE("HurtboxComponent::apply_damage with hitbox multiplier", "[physics][hurtbox]") {
    HurtboxComponent hurtbox;
    hurtbox.max_health = 100.0f;
    hurtbox.current_health = 100.0f;
    hurtbox.armor = 0.0f;

    SECTION("Hitbox with 2x multiplier doubles damage") {
        Hitbox headshot;
        headshot.type = HitboxType::Head;
        headshot.damage_multiplier = 2.0f;

        f32 dealt = hurtbox.apply_damage(25.0f, HitboxType::Head, glm::vec3(0), &headshot);

        REQUIRE(dealt == Approx(50.0f));
        REQUIRE(hurtbox.current_health == Approx(50.0f));
    }

    SECTION("Hitbox with 0.5x multiplier halves damage") {
        Hitbox legshot;
        legshot.type = HitboxType::LeftLeg;
        legshot.damage_multiplier = 0.5f;

        f32 dealt = hurtbox.apply_damage(40.0f, HitboxType::LeftLeg, glm::vec3(0), &legshot);

        REQUIRE(dealt == Approx(20.0f));
        REQUIRE(hurtbox.current_health == Approx(80.0f));
    }

    SECTION("nullptr hitbox uses base damage") {
        f32 dealt = hurtbox.apply_damage(30.0f, HitboxType::Torso, glm::vec3(0), nullptr);

        REQUIRE(dealt == Approx(30.0f));
        REQUIRE(hurtbox.current_health == Approx(70.0f));
    }
}

// ============================================================================
// HitboxComponent Tests
// ============================================================================

TEST_CASE("HitboxComponent default state", "[physics][hitbox]") {
    HitboxComponent comp;

    SECTION("Empty hitboxes by default") {
        REQUIRE(comp.hitboxes.empty());
    }

    SECTION("Empty bone names by default") {
        REQUIRE(comp.bone_names.empty());
    }
}

TEST_CASE("HitboxComponent::create_humanoid", "[physics][hitbox]") {
    auto humanoid = HitboxComponent::create_humanoid();

    SECTION("Has multiple hitboxes") {
        REQUIRE(humanoid.hitboxes.size() >= 6); // At least head, torso, 4 limbs
    }

    SECTION("Has a head hitbox") {
        bool has_head = false;
        for (const auto& hb : humanoid.hitboxes) {
            if (hb.type == HitboxType::Head) {
                has_head = true;
                break;
            }
        }
        REQUIRE(has_head);
    }

    SECTION("Has a torso hitbox") {
        bool has_torso = false;
        for (const auto& hb : humanoid.hitboxes) {
            if (hb.type == HitboxType::Torso) {
                has_torso = true;
                break;
            }
        }
        REQUIRE(has_torso);
    }

    SECTION("Head has higher damage multiplier than torso") {
        f32 head_mult = 1.0f;
        f32 torso_mult = 1.0f;

        for (const auto& hb : humanoid.hitboxes) {
            if (hb.type == HitboxType::Head)
                head_mult = hb.damage_multiplier;
            if (hb.type == HitboxType::Torso)
                torso_mult = hb.damage_multiplier;
        }

        REQUIRE(head_mult > torso_mult);
    }

    SECTION("All hitboxes are enabled") {
        for (const auto& hb : humanoid.hitboxes) {
            REQUIRE(hb.enabled == true);
        }
    }
}

// ============================================================================
// DamageEvent Tests
// ============================================================================

TEST_CASE("DamageEvent default values", "[physics][damage]") {
    DamageEvent event;

    SECTION("Target is null by default") {
        REQUIRE((event.target == entt::null));
    }

    SECTION("Instigator is null by default") {
        REQUIRE((event.instigator == entt::null));
    }

    SECTION("Damage values are zero") {
        REQUIRE(event.damage_amount == Approx(0.0f));
        REQUIRE(event.actual_damage == Approx(0.0f));
    }

    SECTION("Default hit location is Torso") {
        REQUIRE(event.hit_location == HitboxType::Torso);
    }

    SECTION("Hit vectors are zero") {
        REQUIRE(event.hit_point == glm::vec3(0.0f));
        REQUIRE(event.hit_normal == glm::vec3(0.0f));
        REQUIRE(event.damage_direction == glm::vec3(0.0f));
    }
}
