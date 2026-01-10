#pragma once

/**
 * @file cubemap.hpp
 * @brief OpenGL Cubemap texture wrapper for skyboxes
 */

#include "engine/core/types.hpp"
#include "engine/renderer/opengl/gl_context.hpp"

#include <array>
#include <string>

namespace hz {

/**
 * @brief Cubemap texture for skyboxes
 *
 * Loads 6 face textures and creates a GL_TEXTURE_CUBE_MAP
 */
class Cubemap {
public:
    /**
     * @brief Load a cubemap from 6 face images
     * @param faces Array of paths: [right, left, top, bottom, front, back]
     */
    explicit Cubemap(const std::array<std::string, 6>& faces);
    ~Cubemap();

    HZ_NON_COPYABLE(Cubemap);
    HZ_DEFAULT_MOVABLE(Cubemap);

    void bind(u32 slot = 0) const;
    void unbind() const;

    [[nodiscard]] GLuint id() const { return m_texture_id; }

private:
    GLuint m_texture_id{0};
};

} // namespace hz
