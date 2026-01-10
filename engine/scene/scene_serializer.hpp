#pragma once

#include "engine/ecs/world.hpp"

#include <filesystem>
#include <string>

namespace hz {

class SceneSerializer {
public:
    explicit SceneSerializer(World& world);

    /**
     * @brief Serialize the current world to a file
     */
    void serialize(const std::filesystem::path& path);

    /**
     * @brief Deserialize a world from a file (clears current world)
     */
    bool deserialize(const std::filesystem::path& path);

private:
    World& m_world;
};

} // namespace hz
