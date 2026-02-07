#include "texture.hpp"

namespace hz {

Texture::~Texture() noexcept = default;

Texture::Texture(Texture&& other) noexcept = default;
Texture& Texture::operator=(Texture&& other) noexcept = default;

Texture Texture::load_from_file(std::string_view path, const TextureParams& params) {
    // Stub
    Texture tex;
    tex.m_path = std::string(path);
    return tex;
}

Texture Texture::create(u32 width, u32 height, TextureFormat format, const void* data,
                        const TextureParams& params) {
    // Stub
    Texture tex;
    tex.m_width = width;
    tex.m_height = height;
    tex.m_format = format;
    return tex;
}

Texture Texture::load_from_memory(const unsigned char* data, size_t size,
                                  const TextureParams& params) {
    // Stub
    return Texture();
}

} // namespace hz
