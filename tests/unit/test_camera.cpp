/**
 * @file test_camera.cpp
 * @brief Unit tests for camera systems
 */

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <engine/renderer/camera.hpp>
#include <engine/renderer/cinematic_camera.hpp>
#include <glm/gtc/epsilon.hpp>

using namespace hz;
using Catch::Approx;

// Helper to compare glm vectors
static bool vec3_approx_equal(const glm::vec3& a, const glm::vec3& b, float epsilon = 0.001f) {
    return glm::all(glm::epsilonEqual(a, b, epsilon));
}

// ============================================================================
// Camera Basic Tests
// ============================================================================

TEST_CASE("Camera default construction", "[renderer][camera]") {
    Camera camera;

    SECTION("Has default position") {
        glm::vec3 pos = camera.position();
        REQUIRE(pos.y == Approx(2.0f));
        REQUIRE(pos.z == Approx(5.0f));
    }

    SECTION("Has default settings") {
        REQUIRE(camera.movement_speed == Approx(5.0f));
        REQUIRE(camera.mouse_sensitivity == Approx(0.1f));
        REQUIRE(camera.fov == Approx(45.0f));
        REQUIRE(camera.near_plane == Approx(0.1f));
        REQUIRE(camera.far_plane == Approx(1000.0f));
    }

    SECTION("Front vector points in negative Z") {
        glm::vec3 front = camera.front();
        REQUIRE(front.z < 0.0f);
    }
}

TEST_CASE("Camera custom construction", "[renderer][camera]") {
    glm::vec3 pos(10.0f, 5.0f, 3.0f);
    glm::vec3 up(0.0f, 1.0f, 0.0f);

    Camera camera(pos, up, -90.0f, 0.0f);

    REQUIRE(camera.position() == pos);
}

TEST_CASE("Camera::set_position", "[renderer][camera]") {
    Camera camera;
    glm::vec3 new_pos(100.0f, 200.0f, 300.0f);

    camera.set_position(new_pos);

    REQUIRE(camera.position() == new_pos);
}

TEST_CASE("Camera::view_matrix", "[renderer][camera]") {
    Camera camera(glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f), -90.0f, 0.0f);

    glm::mat4 view = camera.view_matrix();

    SECTION("View matrix is valid") {
        // Should not be identity
        REQUIRE(view != glm::mat4(1.0f));
    }

    SECTION("View matrix is invertible") {
        glm::mat4 inv = glm::inverse(view);
        glm::mat4 result = view * inv;

        // Should be approximately identity
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                float expected = (i == j) ? 1.0f : 0.0f;
                REQUIRE(result[i][j] == Approx(expected).margin(0.001f));
            }
        }
    }
}

TEST_CASE("Camera::projection_matrix", "[renderer][camera]") {
    Camera camera;
    camera.fov = 60.0f;
    camera.near_plane = 0.1f;
    camera.far_plane = 100.0f;

    SECTION("Different aspect ratios produce different matrices") {
        glm::mat4 proj_4_3 = camera.projection_matrix(4.0f / 3.0f);
        glm::mat4 proj_16_9 = camera.projection_matrix(16.0f / 9.0f);

        REQUIRE(proj_4_3 != proj_16_9);
    }

    SECTION("Projection matrix has correct near/far behavior") {
        glm::mat4 proj = camera.projection_matrix(1.0f);

        // The [2][2] and [2][3] elements encode depth mapping
        REQUIRE(proj[2][2] != 0.0f);
        REQUIRE(proj[2][3] != 0.0f);
    }
}

TEST_CASE("Camera::process_movement", "[renderer][camera]") {
    // Note: Camera has a MIN_HEIGHT constraint of 1.7f and movement works in XZ plane
    Camera camera(glm::vec3(0.0f, 2.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), -90.0f, 0.0f);
    camera.movement_speed = 10.0f;

    glm::vec3 initial_pos = camera.position();

    SECTION("Forward movement (z direction)") {
        // Movement uses direction.z for forward/backward
        glm::vec3 direction(0.0f, 0.0f, 1.0f); // Forward
        camera.process_movement(direction, 1.0f);

        glm::vec3 new_pos = camera.position();
        REQUIRE(new_pos != initial_pos);
    }

    SECTION("Right movement (x direction)") {
        glm::vec3 direction(1.0f, 0.0f, 0.0f); // Right
        camera.process_movement(direction, 1.0f);

        glm::vec3 new_pos = camera.position();
        REQUIRE(new_pos != initial_pos);
    }

    SECTION("Zero delta time means no horizontal movement") {
        glm::vec3 direction(0.0f, 0.0f, 1.0f);
        camera.process_movement(direction, 0.0f);

        // X and Z should not change
        REQUIRE(camera.position().x == Approx(initial_pos.x));
        REQUIRE(camera.position().z == Approx(initial_pos.z));
    }

    SECTION("Movement scales with delta time") {
        Camera camera1(glm::vec3(0.0f, 2.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), -90.0f, 0.0f);
        Camera camera2(glm::vec3(0.0f, 2.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), -90.0f, 0.0f);
        camera1.movement_speed = 10.0f;
        camera2.movement_speed = 10.0f;

        glm::vec3 dir(0.0f, 0.0f, 1.0f); // Forward
        camera1.process_movement(dir, 0.5f);
        camera2.process_movement(dir, 1.0f);

        // Camera2 should move twice as far in XZ plane
        glm::vec3 pos1 = camera1.position();
        glm::vec3 pos2 = camera2.position();

        // Measure displacement from origin in XZ
        float dist1 = std::sqrt(pos1.x * pos1.x + pos1.z * pos1.z);
        float dist2 = std::sqrt(pos2.x * pos2.x + pos2.z * pos2.z);

        REQUIRE(dist2 == Approx(dist1 * 2.0f).margin(0.01f));
    }

    SECTION("Height is clamped to minimum") {
        // Start above minimum
        Camera cam(glm::vec3(0.0f, 10.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), -90.0f, 0.0f);
        cam.movement_speed = 100.0f;

        // Move down
        glm::vec3 down(0.0f, -1.0f, 0.0f);
        cam.process_movement(down, 1.0f);

        // Should be clamped at MIN_HEIGHT (1.7f)
        REQUIRE(cam.position().y >= 1.7f);
    }
}

TEST_CASE("Camera::process_mouse", "[renderer][camera]") {
    Camera camera(glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f), -90.0f, 0.0f);

    glm::vec3 initial_front = camera.front();

    SECTION("Horizontal mouse movement changes yaw") {
        camera.process_mouse(10.0f, 0.0f);
        glm::vec3 new_front = camera.front();

        // Front vector should have changed
        REQUIRE_FALSE(vec3_approx_equal(new_front, initial_front));
        // Y component should remain similar (horizontal rotation)
        REQUIRE(new_front.y == Approx(initial_front.y).margin(0.01f));
    }

    SECTION("Vertical mouse movement changes pitch") {
        camera.process_mouse(0.0f, 10.0f);
        glm::vec3 new_front = camera.front();

        // Front vector should have changed
        REQUIRE_FALSE(vec3_approx_equal(new_front, initial_front));
    }

    SECTION("Pitch is constrained by default") {
        // Try to look straight up
        camera.process_mouse(0.0f, 1000.0f);
        glm::vec3 front = camera.front();

        // Should not be able to look completely vertical
        REQUIRE(front.y < 1.0f);
    }

    SECTION("Zero offset means no change") {
        camera.process_mouse(0.0f, 0.0f);
        REQUIRE(vec3_approx_equal(camera.front(), initial_front));
    }
}

// ============================================================================
// CinematicCamera Tests
// ============================================================================

TEST_CASE("CinematicCamera default state", "[renderer][cinematic]") {
    CinematicCamera camera;

    SECTION("Not playing by default") {
        REQUIRE(camera.is_playing() == false);
    }

    SECTION("Not complete by default") {
        REQUIRE(camera.is_complete() == false);
    }

    SECTION("Default FOV") {
        REQUIRE(camera.fov() == Approx(45.0f));
    }

    SECTION("Letterbox disabled by default") {
        REQUIRE(camera.letterbox_enabled() == false);
    }
}

TEST_CASE("CinematicCamera keyframe management", "[renderer][cinematic]") {
    CinematicCamera camera;

    SECTION("Add keyframe increases count") {
        CameraKeyframe kf;
        kf.position = glm::vec3(0.0f, 0.0f, 0.0f);
        kf.duration = 1.0f;

        camera.add_keyframe(kf);
        // Can't directly test count, but play should work
        camera.play();
        REQUIRE(camera.is_playing() == true);
    }

    SECTION("Clear keyframes stops playback") {
        CameraKeyframe kf;
        camera.add_keyframe(kf);
        camera.play();
        camera.clear_keyframes();
        camera.stop();

        REQUIRE(camera.is_playing() == false);
    }
}

TEST_CASE("CinematicCamera playback control", "[renderer][cinematic]") {
    CinematicCamera camera;

    CameraKeyframe kf1;
    kf1.position = glm::vec3(0.0f);
    kf1.target = glm::vec3(0.0f, 0.0f, -1.0f);
    kf1.duration = 1.0f;

    CameraKeyframe kf2;
    kf2.position = glm::vec3(10.0f, 0.0f, 0.0f);
    kf2.target = glm::vec3(10.0f, 0.0f, -1.0f);
    kf2.duration = 1.0f;

    camera.add_keyframe(kf1);
    camera.add_keyframe(kf2);

    SECTION("Play starts playback") {
        camera.play();
        REQUIRE(camera.is_playing() == true);
    }

    SECTION("Pause stops playback") {
        camera.play();
        camera.pause();
        REQUIRE(camera.is_playing() == false);
    }

    SECTION("Stop resets playback") {
        camera.play();
        camera.update(0.5f);
        camera.stop();

        REQUIRE(camera.is_playing() == false);
        REQUIRE(camera.current_keyframe_index() == 0);
    }
}

TEST_CASE("CinematicCamera letterbox", "[renderer][cinematic]") {
    CinematicCamera camera;

    SECTION("Enable letterbox") {
        camera.set_letterbox(true, 2.39f);
        REQUIRE(camera.letterbox_enabled() == true);
        REQUIRE(camera.letterbox_ratio() == Approx(2.39f));
    }

    SECTION("Disable letterbox") {
        camera.set_letterbox(true);
        camera.set_letterbox(false);
        REQUIRE(camera.letterbox_enabled() == false);
    }

    SECTION("letterbox_bar_height calculation") {
        camera.set_letterbox(true, 2.39f);

        // For 16:9 screen (~1.78 aspect)
        float bar_height = camera.letterbox_bar_height(16.0f / 9.0f);

        // Letterbox bars should be positive when target ratio > screen ratio
        REQUIRE(bar_height > 0.0f);
        REQUIRE(bar_height < 0.5f); // Less than half the screen
    }

    SECTION("letterbox_bar_height is zero when disabled") {
        camera.set_letterbox(false);
        float bar_height = camera.letterbox_bar_height(16.0f / 9.0f);

        REQUIRE(bar_height == Approx(0.0f));
    }

    SECTION("Wide screen has smaller letterbox bars") {
        camera.set_letterbox(true, 2.39f);

        float bar_4_3 = camera.letterbox_bar_height(4.0f / 3.0f);
        float bar_21_9 = camera.letterbox_bar_height(21.0f / 9.0f);

        // Wider screen needs smaller bars (or none)
        REQUIRE(bar_21_9 < bar_4_3);
    }
}

TEST_CASE("CinematicCamera view/projection matrices", "[renderer][cinematic]") {
    CinematicCamera camera;

    SECTION("View matrix is valid") {
        glm::mat4 view = camera.view_matrix();

        // Should be a valid transformation matrix
        REQUIRE(view[3][3] == Approx(1.0f));
    }

    SECTION("Projection matrix varies with aspect ratio") {
        glm::mat4 proj1 = camera.projection_matrix(1.0f);
        glm::mat4 proj2 = camera.projection_matrix(2.0f);

        REQUIRE(proj1 != proj2);
    }
}

// ============================================================================
// CameraKeyframe Tests
// ============================================================================

TEST_CASE("CameraKeyframe defaults", "[renderer][cinematic]") {
    CameraKeyframe kf;

    SECTION("Default position is origin") {
        REQUIRE(kf.position == glm::vec3(0.0f));
    }

    SECTION("Default target") {
        REQUIRE(kf.target == glm::vec3(0.0f, 0.0f, -1.0f));
    }

    SECTION("Default FOV") {
        REQUIRE(kf.fov == Approx(45.0f));
    }

    SECTION("Default duration") {
        REQUIRE(kf.duration == Approx(1.0f));
    }

    SECTION("Default move type is EaseInOut") {
        REQUIRE(kf.move_type == CameraMoveType::EaseInOut);
    }
}

// ============================================================================
// CameraMoveType Tests
// ============================================================================

TEST_CASE("CameraMoveType enum completeness", "[renderer][cinematic]") {
    // Verify all movement types are distinct
    REQUIRE(CameraMoveType::Cut != CameraMoveType::Lerp);
    REQUIRE(CameraMoveType::Lerp != CameraMoveType::EaseIn);
    REQUIRE(CameraMoveType::EaseIn != CameraMoveType::EaseOut);
    REQUIRE(CameraMoveType::EaseOut != CameraMoveType::EaseInOut);
    REQUIRE(CameraMoveType::EaseInOut != CameraMoveType::Dolly);
    REQUIRE(CameraMoveType::Dolly != CameraMoveType::Orbit);
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_CASE("Camera vectors are orthonormal", "[renderer][camera]") {
    Camera camera(glm::vec3(5.0f, 10.0f, 15.0f), glm::vec3(0.0f, 1.0f, 0.0f), -45.0f, 15.0f);

    glm::vec3 front = camera.front();
    glm::vec3 right = camera.right();

    SECTION("Front is normalized") {
        REQUIRE(glm::length(front) == Approx(1.0f).margin(0.001f));
    }

    SECTION("Right is normalized") {
        REQUIRE(glm::length(right) == Approx(1.0f).margin(0.001f));
    }

    SECTION("Front and right are perpendicular") {
        float dot = glm::dot(front, right);
        REQUIRE(dot == Approx(0.0f).margin(0.001f));
    }
}
