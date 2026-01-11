#include "terrain.hpp"

#include "engine/core/log.hpp"
#include "opengl/gl_context.hpp"

#include <cmath>

#include <stb_image.h>

namespace hz {

bool Terrain::generate_from_heightmap(const std::string& heightmap_path, const TerrainConfig& config) {
    m_config = config;

    // Load heightmap
    int width, height, channels;
    stbi_set_flip_vertically_on_load(false);
    unsigned char* data = stbi_load(heightmap_path.c_str(), &width, &height, &channels, 1);

    if (!data) {
        HZ_ENGINE_ERROR("Failed to load heightmap: {}", heightmap_path);
        return false;
    }

    m_heightmap_width = static_cast<u32>(width);
    m_heightmap_depth = static_cast<u32>(height);

    HZ_ENGINE_INFO("Loaded heightmap: {}x{}", width, height);

    // Cache heightmap data for get_height_at()
    m_heightmap_data.resize(static_cast<size_t>(width * height));
    for (int i = 0; i < width * height; ++i) {
        m_heightmap_data[static_cast<size_t>(i)] = static_cast<float>(data[i]) / 255.0f;
    }

    // Generate vertices
    std::vector<TerrainVertex> vertices;
    vertices.reserve(static_cast<size_t>(width * height));

    float half_width = config.width / 2.0f;
    float half_depth = config.depth / 2.0f;

    for (int z = 0; z < height; ++z) {
        for (int x = 0; x < width; ++x) {
            float height_value = m_heightmap_data[static_cast<size_t>(z * width + x)];

            TerrainVertex vertex;
            
            // Position: centered at origin
            float px = (static_cast<float>(x) / (width - 1)) * config.width - half_width;
            float pz = (static_cast<float>(z) / (height - 1)) * config.depth - half_depth;
            float py = height_value * config.max_height;
            
            vertex.position = glm::vec3(px, py, pz);
            vertex.normal = glm::vec3(0.0f, 1.0f, 0.0f); // Will be calculated later
            
            // Detail texture coords (tiled)
            vertex.texcoord = glm::vec2(
                static_cast<float>(x) / (width - 1) * config.texture_scale,
                static_cast<float>(z) / (height - 1) * config.texture_scale
            );
            
            // Splatmap coords (0-1 range)
            vertex.splatcoord = glm::vec2(
                static_cast<float>(x) / (width - 1),
                static_cast<float>(z) / (height - 1)
            );

            vertices.push_back(vertex);
        }
    }

    stbi_image_free(data);

    // Generate indices (triangle strip converted to triangles)
    std::vector<u32> indices;
    indices.reserve(static_cast<size_t>((width - 1) * (height - 1) * 6));

    for (int z = 0; z < height - 1; ++z) {
        for (int x = 0; x < width - 1; ++x) {
            u32 top_left = static_cast<u32>(z * width + x);
            u32 top_right = top_left + 1;
            u32 bottom_left = static_cast<u32>((z + 1) * width + x);
            u32 bottom_right = bottom_left + 1;

            // First triangle
            indices.push_back(top_left);
            indices.push_back(bottom_left);
            indices.push_back(top_right);

            // Second triangle
            indices.push_back(top_right);
            indices.push_back(bottom_left);
            indices.push_back(bottom_right);
        }
    }

    // Calculate normals
    calculate_normals(vertices, indices, static_cast<u32>(width), static_cast<u32>(height));

    // Upload to GPU
    upload_mesh(vertices, indices);

    HZ_ENGINE_INFO("Generated terrain: {}x{} vertices, {} triangles", 
                   width, height, indices.size() / 3);

    return true;
}

void Terrain::generate_flat(const TerrainConfig& config) {
    m_config = config;
    m_heightmap_width = config.resolution;
    m_heightmap_depth = config.resolution;

    // Create flat heightmap
    m_heightmap_data.resize(static_cast<size_t>(config.resolution * config.resolution), 0.0f);

    std::vector<TerrainVertex> vertices;
    vertices.reserve(static_cast<size_t>(config.resolution * config.resolution));

    float half_width = config.width / 2.0f;
    float half_depth = config.depth / 2.0f;

    for (u32 z = 0; z < config.resolution; ++z) {
        for (u32 x = 0; x < config.resolution; ++x) {
            TerrainVertex vertex;
            
            float px = (static_cast<float>(x) / (config.resolution - 1)) * config.width - half_width;
            float pz = (static_cast<float>(z) / (config.resolution - 1)) * config.depth - half_depth;
            
            vertex.position = glm::vec3(px, 0.0f, pz);
            vertex.normal = glm::vec3(0.0f, 1.0f, 0.0f);
            vertex.texcoord = glm::vec2(
                static_cast<float>(x) / (config.resolution - 1) * config.texture_scale,
                static_cast<float>(z) / (config.resolution - 1) * config.texture_scale
            );
            vertex.splatcoord = glm::vec2(
                static_cast<float>(x) / (config.resolution - 1),
                static_cast<float>(z) / (config.resolution - 1)
            );

            vertices.push_back(vertex);
        }
    }

    // Generate indices
    std::vector<u32> indices;
    for (u32 z = 0; z < config.resolution - 1; ++z) {
        for (u32 x = 0; x < config.resolution - 1; ++x) {
            u32 top_left = z * config.resolution + x;
            u32 top_right = top_left + 1;
            u32 bottom_left = (z + 1) * config.resolution + x;
            u32 bottom_right = bottom_left + 1;

            indices.push_back(top_left);
            indices.push_back(bottom_left);
            indices.push_back(top_right);

            indices.push_back(top_right);
            indices.push_back(bottom_left);
            indices.push_back(bottom_right);
        }
    }

    upload_mesh(vertices, indices);
    HZ_ENGINE_INFO("Generated flat terrain: {}x{}", config.resolution, config.resolution);
}

void Terrain::generate_procedural(const TerrainConfig& config, u32 seed, 
                                   u32 octaves, float persistence) {
    m_config = config;
    m_heightmap_width = config.resolution;
    m_heightmap_depth = config.resolution;

    m_heightmap_data.resize(static_cast<size_t>(config.resolution * config.resolution));

    std::vector<TerrainVertex> vertices;
    vertices.reserve(static_cast<size_t>(config.resolution * config.resolution));

    float half_width = config.width / 2.0f;
    float half_depth = config.depth / 2.0f;

    for (u32 z = 0; z < config.resolution; ++z) {
        for (u32 x = 0; x < config.resolution; ++x) {
            // Generate height using Perlin noise
            float nx = static_cast<float>(x) / config.resolution * 4.0f;
            float nz = static_cast<float>(z) / config.resolution * 4.0f;
            float height_value = perlin2d(nx, nz, seed, octaves, persistence);
            height_value = (height_value + 1.0f) * 0.5f; // Normalize to 0-1

            m_heightmap_data[z * config.resolution + x] = height_value;

            TerrainVertex vertex;
            
            float px = (static_cast<float>(x) / (config.resolution - 1)) * config.width - half_width;
            float pz = (static_cast<float>(z) / (config.resolution - 1)) * config.depth - half_depth;
            float py = height_value * config.max_height;
            
            vertex.position = glm::vec3(px, py, pz);
            vertex.normal = glm::vec3(0.0f, 1.0f, 0.0f);
            vertex.texcoord = glm::vec2(
                static_cast<float>(x) / (config.resolution - 1) * config.texture_scale,
                static_cast<float>(z) / (config.resolution - 1) * config.texture_scale
            );
            vertex.splatcoord = glm::vec2(
                static_cast<float>(x) / (config.resolution - 1),
                static_cast<float>(z) / (config.resolution - 1)
            );

            vertices.push_back(vertex);
        }
    }

    // Generate indices
    std::vector<u32> indices;
    for (u32 z = 0; z < config.resolution - 1; ++z) {
        for (u32 x = 0; x < config.resolution - 1; ++x) {
            u32 top_left = z * config.resolution + x;
            u32 top_right = top_left + 1;
            u32 bottom_left = (z + 1) * config.resolution + x;
            u32 bottom_right = bottom_left + 1;

            indices.push_back(top_left);
            indices.push_back(bottom_left);
            indices.push_back(top_right);

            indices.push_back(top_right);
            indices.push_back(bottom_left);
            indices.push_back(bottom_right);
        }
    }

    calculate_normals(vertices, indices, config.resolution, config.resolution);
    upload_mesh(vertices, indices);

    HZ_ENGINE_INFO("Generated procedural terrain: {}x{} with {} octaves", 
                   config.resolution, config.resolution, octaves);
}

void Terrain::draw() const {
    if (m_vao == 0) return;

    glBindVertexArray(m_vao);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(m_index_count), GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

float Terrain::get_height_at(float x, float z) const {
    if (m_heightmap_data.empty()) return 0.0f;

    // Convert world coords to heightmap coords
    float half_width = m_config.width / 2.0f;
    float half_depth = m_config.depth / 2.0f;

    float hx = (x + half_width) / m_config.width * (m_heightmap_width - 1);
    float hz = (z + half_depth) / m_config.depth * (m_heightmap_depth - 1);

    // Clamp to bounds
    hx = glm::clamp(hx, 0.0f, static_cast<float>(m_heightmap_width - 1));
    hz = glm::clamp(hz, 0.0f, static_cast<float>(m_heightmap_depth - 1));

    // Bilinear interpolation
    int x0 = static_cast<int>(hx);
    int z0 = static_cast<int>(hz);
    int x1 = glm::min(x0 + 1, static_cast<int>(m_heightmap_width - 1));
    int z1 = glm::min(z0 + 1, static_cast<int>(m_heightmap_depth - 1));

    float fx = hx - x0;
    float fz = hz - z0;

    float h00 = m_heightmap_data[static_cast<size_t>(z0 * m_heightmap_width + x0)];
    float h10 = m_heightmap_data[static_cast<size_t>(z0 * m_heightmap_width + x1)];
    float h01 = m_heightmap_data[static_cast<size_t>(z1 * m_heightmap_width + x0)];
    float h11 = m_heightmap_data[static_cast<size_t>(z1 * m_heightmap_width + x1)];

    float h0 = glm::mix(h00, h10, fx);
    float h1 = glm::mix(h01, h11, fx);

    return glm::mix(h0, h1, fz) * m_config.max_height;
}

void Terrain::calculate_normals(std::vector<TerrainVertex>& vertices, 
                                 const std::vector<u32>& indices,
                                 u32 grid_width, u32 grid_depth) {
    // Reset normals
    for (auto& v : vertices) {
        v.normal = glm::vec3(0.0f);
    }

    // Calculate face normals and accumulate
    for (size_t i = 0; i < indices.size(); i += 3) {
        u32 i0 = indices[i];
        u32 i1 = indices[i + 1];
        u32 i2 = indices[i + 2];

        glm::vec3 v0 = vertices[i0].position;
        glm::vec3 v1 = vertices[i1].position;
        glm::vec3 v2 = vertices[i2].position;

        glm::vec3 edge1 = v1 - v0;
        glm::vec3 edge2 = v2 - v0;
        glm::vec3 normal = glm::cross(edge1, edge2);

        vertices[i0].normal += normal;
        vertices[i1].normal += normal;
        vertices[i2].normal += normal;
    }

    // Normalize
    for (auto& v : vertices) {
        v.normal = glm::normalize(v.normal);
    }
}

void Terrain::upload_mesh(const std::vector<TerrainVertex>& vertices, 
                          const std::vector<u32>& indices) {
    // Clean up old buffers
    if (m_vao) glDeleteVertexArrays(1, &m_vao);
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    if (m_ebo) glDeleteBuffers(1, &m_ebo);

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glGenBuffers(1, &m_ebo);

    glBindVertexArray(m_vao);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, 
                 static_cast<GLsizeiptr>(vertices.size() * sizeof(TerrainVertex)),
                 vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(indices.size() * sizeof(u32)),
                 indices.data(), GL_STATIC_DRAW);

    // Position (location 0)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex),
                          reinterpret_cast<void*>(offsetof(TerrainVertex, position)));

    // Normal (location 1)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex),
                          reinterpret_cast<void*>(offsetof(TerrainVertex, normal)));

    // Texcoord (location 2)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex),
                          reinterpret_cast<void*>(offsetof(TerrainVertex, texcoord)));

    // Splatcoord (location 3)
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(TerrainVertex),
                          reinterpret_cast<void*>(offsetof(TerrainVertex, splatcoord)));

    glBindVertexArray(0);

    m_index_count = static_cast<u32>(indices.size());
}

// Simple hash-based noise
float Terrain::noise2d(float x, float y, u32 seed) {
    int xi = static_cast<int>(std::floor(x));
    int yi = static_cast<int>(std::floor(y));
    
    auto hash = [seed](int x, int y) -> float {
        int n = x + y * 57 + seed * 131;
        n = (n << 13) ^ n;
        return (1.0f - ((n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0f);
    };

    float fx = x - xi;
    float fy = y - yi;

    // Smoothstep
    fx = fx * fx * (3 - 2 * fx);
    fy = fy * fy * (3 - 2 * fy);

    float n00 = hash(xi, yi);
    float n10 = hash(xi + 1, yi);
    float n01 = hash(xi, yi + 1);
    float n11 = hash(xi + 1, yi + 1);

    float n0 = glm::mix(n00, n10, fx);
    float n1 = glm::mix(n01, n11, fx);

    return glm::mix(n0, n1, fy);
}

float Terrain::perlin2d(float x, float y, u32 seed, u32 octaves, float persistence) {
    float total = 0.0f;
    float frequency = 1.0f;
    float amplitude = 1.0f;
    float max_value = 0.0f;

    for (u32 i = 0; i < octaves; ++i) {
        total += noise2d(x * frequency, y * frequency, seed + i) * amplitude;
        max_value += amplitude;
        amplitude *= persistence;
        frequency *= 2.0f;
    }

    return total / max_value;
}

} // namespace hz
