#include "cubemap.hpp"

#include "engine/core/log.hpp"

#define STB_IMAGE_IMPLEMENTATION_ALREADY_DONE
#include <stb_image.h>

namespace hz {

Cubemap::Cubemap(const std::array<std::string, 6>& faces) {
    glGenTextures(1, &m_texture_id);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_texture_id);

    // Face order: right, left, top, bottom, front, back
    // GL order: +X, -X, +Y, -Y, +Z, -Z
    for (u32 i = 0; i < 6; ++i) {
        int width, height, channels;
        stbi_set_flip_vertically_on_load(false); // Cubemaps are not flipped
        unsigned char* data = stbi_load(faces[i].c_str(), &width, &height, &channels, 0);

        if (data) {
            GLenum format = (channels == 4) ? GL_RGBA : GL_RGB;
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, format, width, height, 0, format,
                         GL_UNSIGNED_BYTE, data);
            stbi_image_free(data);
        } else {
            HZ_ENGINE_ERROR("Failed to load cubemap face: {}", faces[i]);
            stbi_image_free(data);
        }
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    HZ_ENGINE_INFO("Loaded cubemap with {} faces", 6);
}

Cubemap::~Cubemap() noexcept {
    if (m_texture_id) {
        glDeleteTextures(1, &m_texture_id);
    }
}

void Cubemap::bind(u32 slot) const {
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_texture_id);
}

void Cubemap::unbind() const {
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
}

} // namespace hz
