#include "texture.hpp"

#include "engine/vendor/glad/glad.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace hz {

Texture::~Texture() {
    if (m_id != 0) {
        glDeleteTextures(1, &m_id);
        HZ_ENGINE_TRACE("Texture {} destroyed", m_id);
    }
}

Texture::Texture(Texture&& other) noexcept
    : m_id(other.m_id)
    , m_width(other.m_width)
    , m_height(other.m_height)
    , m_format(other.m_format)
    , m_path(std::move(other.m_path)) {
    other.m_id = 0;
}

Texture& Texture::operator=(Texture&& other) noexcept {
    if (this != &other) {
        if (m_id != 0) {
            glDeleteTextures(1, &m_id);
        }
        m_id = other.m_id;
        m_width = other.m_width;
        m_height = other.m_height;
        m_format = other.m_format;
        m_path = std::move(other.m_path);
        other.m_id = 0;
    }
    return *this;
}

namespace {

GLenum to_gl_filter(TextureFilter filter, bool is_min) {
    switch (filter) {
    case TextureFilter::Nearest:
        return GL_NEAREST;
    case TextureFilter::Linear:
        return GL_LINEAR;
    case TextureFilter::NearestMipmap:
        return is_min ? GL_NEAREST_MIPMAP_NEAREST : GL_NEAREST;
    case TextureFilter::LinearMipmap:
        return is_min ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR;
    }
    return GL_LINEAR;
}

GLenum to_gl_wrap(TextureWrap wrap) {
    switch (wrap) {
    case TextureWrap::Repeat:
        return GL_REPEAT;
    case TextureWrap::MirroredRepeat:
        return GL_MIRRORED_REPEAT;
    case TextureWrap::ClampToEdge:
        return GL_CLAMP_TO_EDGE;
    }
    return GL_REPEAT;
}

GLenum to_gl_internal_format(TextureFormat format) {
    switch (format) {
    case TextureFormat::R8:
        return GL_R8;
    case TextureFormat::RG8:
        return GL_RG8;
    case TextureFormat::RGB8:
        return GL_RGB8;
    case TextureFormat::RGBA8:
        return GL_RGBA8;
    case TextureFormat::SRGB8:
        return GL_SRGB8;
    case TextureFormat::SRGBA8:
        return GL_SRGB8_ALPHA8;
    }
    return GL_RGBA8;
}

GLenum to_gl_format(TextureFormat format) {
    switch (format) {
    case TextureFormat::R8:
        return GL_RED;
    case TextureFormat::RG8:
        return GL_RG;
    case TextureFormat::RGB8:
    case TextureFormat::SRGB8:
        return GL_RGB;
    case TextureFormat::RGBA8:
    case TextureFormat::SRGBA8:
        return GL_RGBA;
    }
    return GL_RGBA;
}

} // anonymous namespace

Texture Texture::load_from_file(std::string_view path, const TextureParams& params) {
    // Load image with stb_image
    stbi_set_flip_vertically_on_load(true);

    int width, height, channels;
    unsigned char* data = stbi_load(std::string(path).c_str(), &width, &height, &channels, 0);

    if (!data) {
        HZ_ENGINE_ERROR("Failed to load texture: {}", path);
        return {};
    }

    // Determine format
    TextureFormat format;
    switch (channels) {
    case 1:
        format = TextureFormat::R8;
        break;
    case 2:
        format = TextureFormat::RG8;
        break;
    case 3:
        format = params.srgb ? TextureFormat::SRGB8 : TextureFormat::RGB8;
        break;
    case 4:
    default:
        format = params.srgb ? TextureFormat::SRGBA8 : TextureFormat::RGBA8;
        break;
    }

    Texture tex = create(static_cast<u32>(width), static_cast<u32>(height), format, data, params);
    tex.m_path = path;

    stbi_image_free(data);

    HZ_ENGINE_INFO("Loaded texture: {} ({}x{}, {} channels)", path, width, height, channels);
    return tex;
}

Texture Texture::create(u32 width, u32 height, TextureFormat format, const void* data,
                        const TextureParams& params) {
    Texture tex;
    tex.m_width = width;
    tex.m_height = height;
    tex.m_format = format;

    glGenTextures(1, &tex.m_id);
    glBindTexture(GL_TEXTURE_2D, tex.m_id);

    // Set parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                    static_cast<GLint>(to_gl_filter(params.min_filter, true)));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                    static_cast<GLint>(to_gl_filter(params.mag_filter, false)));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                    static_cast<GLint>(to_gl_wrap(params.wrap_s)));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                    static_cast<GLint>(to_gl_wrap(params.wrap_t)));

    // Upload data
    glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(to_gl_internal_format(format)),
                 static_cast<GLsizei>(width), static_cast<GLsizei>(height), 0, to_gl_format(format),
                 GL_UNSIGNED_BYTE, data);

    if (params.generate_mipmaps) {
        glGenerateMipmap(GL_TEXTURE_2D);
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

void Texture::bind(u32 unit) const {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, m_id);
}

void Texture::unbind(u32 unit) {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, 0);
}

} // namespace hz
