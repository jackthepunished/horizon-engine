#pragma once

#include "engine/scene/scene.hpp"

#include <filesystem>
#include <string>

namespace hz {

class SceneSerializer {
public:
    explicit SceneSerializer(Scene& scene);

    /**
     * @brief Serialize the current scene to a file
     */
    void serialize(const std::filesystem::path& path);

    /**
     * @brief Deserialize a scene from a file (clears current scene)
     */
    bool deserialize(const std::filesystem::path& path);

private:
    Scene& m_scene;
};

} // namespace hz
