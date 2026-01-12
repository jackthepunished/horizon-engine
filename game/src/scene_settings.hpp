#pragma once

#include <glm/glm.hpp>

namespace game {

struct SceneSettings {
    // Environment
    glm::vec3 clear_color{0.1f, 0.1f, 0.2f};
    glm::vec3 ambient_color{0.4f, 0.5f, 0.6f};   // Brighter ambient
    float ambient_intensity = 1.0f;              // Full ambient

    // Directional Light (Sun) - Noon overhead
    glm::vec3 sun_direction{0.0f, -1.0f, 0.0f};  // Directly overhead
    glm::vec3 sun_color{1.0f, 0.98f, 0.95f};     // Bright white
    float sun_intensity = 6.0f;                   // Strong sunlight

    // Post Processing
    float exposure = 1.5f;       // Increased for brighter scene
    float bloom_intensity = 0.3f; // Slightly reduced bloom
    float bloom_threshold = 0.8f;
    bool bloom_enabled = true;

    // Fog
    bool fog_enabled = false;
    float fog_density = 0.01f;
    float fog_gradient = 1.5f;
};

} // namespace game
