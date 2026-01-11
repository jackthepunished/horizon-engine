#pragma once

#include <glm/glm.hpp>

namespace game {

struct SceneSettings {
    // Environment
    glm::vec3 clear_color{0.1f, 0.1f, 0.2f};
    glm::vec3 ambient_color{0.2f, 0.2f, 0.3f};
    float ambient_intensity = 0.3f; // Lower ambient

    // Directional Light (Sun)
    glm::vec3 sun_direction{0.8f, -0.2f, 0.2f};
    glm::vec3 sun_color{1.0f, 0.8f, 0.6f}; // Softer, more natural light
    float sun_intensity = 1.5f; // Much lower intensity

    // Post Processing
    float exposure = 1.0f;
    float bloom_intensity = 0.4f;
    float bloom_threshold = 0.8f;
    bool bloom_enabled = true;

    // Fog
    bool fog_enabled = false;
    float fog_density = 0.01f;
    float fog_gradient = 1.5f;
};

} // namespace game
