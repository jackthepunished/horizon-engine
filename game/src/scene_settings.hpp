#pragma once

#include <glm/glm.hpp>

namespace game {

struct SceneSettings {
    // Environment
    glm::vec3 clear_color{0.02f, 0.02f, 0.05f};
    glm::vec3 ambient_color{0.1f, 0.1f, 0.15f};
    float ambient_intensity = 0.5f;

    // Directional Light (Sun)
    glm::vec3 sun_direction{-0.5f, -1.0f, -0.3f};
    glm::vec3 sun_color{1.0f, 0.9f, 0.8f};
    float sun_intensity = 2.0f;

    // Post Processing
    float exposure = 1.0f;
    float bloom_intensity = 0.5f;
    float bloom_threshold = 0.8f;
    bool bloom_enabled = true;

    // Fog
    bool fog_enabled = true;
    float fog_density = 0.05f;
    float fog_gradient = 1.5f;
};

} // namespace game
