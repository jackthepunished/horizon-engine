#include "particle_system.hpp"

#include "engine/core/log.hpp"
#include "opengl/gl_context.hpp"

#include <algorithm>
#include <chrono>

#include <glm/gtc/constants.hpp>

namespace hz {

// ============================================================================
// ParticleEmitter
// ============================================================================

ParticleEmitter::~ParticleEmitter() noexcept {
    if (m_vao) {
        glDeleteVertexArrays(1, &m_vao);
        glDeleteBuffers(1, &m_quad_vbo);
        glDeleteBuffers(1, &m_instance_vbo);
    }
}

ParticleEmitter::ParticleEmitter(ParticleEmitter&& other) noexcept {
    *this = std::move(other);
}

ParticleEmitter& ParticleEmitter::operator=(ParticleEmitter&& other) noexcept {
    if (this == &other)
        return *this;

    if (m_vao) {
        glDeleteVertexArrays(1, &m_vao);
        glDeleteBuffers(1, &m_quad_vbo);
        glDeleteBuffers(1, &m_instance_vbo);
    }

    m_config = other.m_config;
    m_particles = std::move(other.m_particles);
    m_instance_data = std::move(other.m_instance_data);
    m_emitting = other.m_emitting;
    m_emit_accumulator = other.m_emit_accumulator;
    m_active_count = other.m_active_count;
    m_rng = std::move(other.m_rng);

    m_vao = other.m_vao;
    m_quad_vbo = other.m_quad_vbo;
    m_instance_vbo = other.m_instance_vbo;

    other.m_vao = 0;
    other.m_quad_vbo = 0;
    other.m_instance_vbo = 0;
    other.m_active_count = 0;
    other.m_emit_accumulator = 0.0f;

    return *this;
}

void ParticleEmitter::init(const ParticleEmitterConfig& config) {
    m_config = config;
    
    // Initialize RNG with time-based seed
    auto seed = static_cast<u32>(
        std::chrono::high_resolution_clock::now().time_since_epoch().count());
    m_rng.seed(seed);
    
    // Pre-allocate particle pool
    m_particles.resize(config.max_particles);
    m_instance_data.reserve(config.max_particles);
    
    create_quad_mesh();
    
    HZ_ENGINE_INFO("Particle emitter initialized: max_particles={}, emit_rate={}", 
                   config.max_particles, config.emit_rate);
}

void ParticleEmitter::create_quad_mesh() {
    // Clean up existing buffers
    if (m_vao) {
        glDeleteVertexArrays(1, &m_vao);
        glDeleteBuffers(1, &m_quad_vbo);
        glDeleteBuffers(1, &m_instance_vbo);
    }

    // Quad vertices (position + texcoord)
    float quad_vertices[] = {
        // Position        // TexCoord
        -0.5f, -0.5f, 0.0f,  0.0f, 0.0f,
         0.5f, -0.5f, 0.0f,  1.0f, 0.0f,
         0.5f,  0.5f, 0.0f,  1.0f, 1.0f,
        
        -0.5f, -0.5f, 0.0f,  0.0f, 0.0f,
         0.5f,  0.5f, 0.0f,  1.0f, 1.0f,
        -0.5f,  0.5f, 0.0f,  0.0f, 1.0f,
    };

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_quad_vbo);
    glGenBuffers(1, &m_instance_vbo);

    glBindVertexArray(m_vao);

    // Quad VBO
    glBindBuffer(GL_ARRAY_BUFFER, m_quad_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), quad_vertices, GL_STATIC_DRAW);

    // Position (location 0)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);

    // TexCoord (location 1)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), 
                          (void*)(3 * sizeof(float)));

    // Instance VBO - allocate for max particles
    glBindBuffer(GL_ARRAY_BUFFER, m_instance_vbo);
    glBufferData(GL_ARRAY_BUFFER, 
                 static_cast<GLsizeiptr>(m_config.max_particles * sizeof(ParticleInstanceData)),
                 nullptr, 
                 GL_DYNAMIC_DRAW);

    // Instance position (location 2)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(ParticleInstanceData), 
                          (void*)offsetof(ParticleInstanceData, position));
    glVertexAttribDivisor(2, 1);

    // Instance color (location 3)
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(ParticleInstanceData), 
                          (void*)offsetof(ParticleInstanceData, color));
    glVertexAttribDivisor(3, 1);

    // Instance size (location 4)
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(ParticleInstanceData), 
                          (void*)offsetof(ParticleInstanceData, size));
    glVertexAttribDivisor(4, 1);

    // Instance rotation (location 5)
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, sizeof(ParticleInstanceData), 
                          (void*)offsetof(ParticleInstanceData, rotation));
    glVertexAttribDivisor(5, 1);

    glBindVertexArray(0);
}

void ParticleEmitter::update(float dt) {
    // Emit new particles
    if (m_emitting && m_config.emit_rate > 0.0f && !m_config.burst_mode) {
        m_emit_accumulator += dt;
        float emit_interval = 1.0f / m_config.emit_rate;
        
        while (m_emit_accumulator >= emit_interval) {
            emit_particle();
            m_emit_accumulator -= emit_interval;
        }
    }
    
    // Update existing particles
    m_active_count = 0;
    m_instance_data.clear();
    
    for (auto& p : m_particles) {
        if (!p.active) continue;
        
        // Update lifetime
        p.life -= dt / p.max_life;
        
        if (p.life <= 0.0f) {
            p.active = false;
            continue;
        }
        
        // Physics update
        p.velocity += m_config.gravity * dt;
        p.velocity *= (1.0f - m_config.drag * dt);
        p.position += p.velocity * dt;
        p.rotation += p.rotation_speed * dt;
        
        // Interpolate properties based on life
        float t = 1.0f - p.life; // 0 = start, 1 = end
        
        glm::vec4 current_color = glm::mix(p.color, p.color_end, t);
        float current_size = glm::mix(p.size, p.size_end, t);
        
        // Add to instance data
        ParticleInstanceData instance;
        instance.position = p.position;
        instance.color = current_color;
        instance.size = current_size;
        instance.rotation = p.rotation;
        m_instance_data.push_back(instance);
        
        ++m_active_count;
    }
    
    // Upload instance data to GPU
    if (m_active_count > 0) {
        upload_instance_data();
    }
}

void ParticleEmitter::upload_instance_data() {
    glBindBuffer(GL_ARRAY_BUFFER, m_instance_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, 
                    static_cast<GLsizeiptr>(m_instance_data.size() * sizeof(ParticleInstanceData)),
                    m_instance_data.data());
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void ParticleEmitter::draw() const {
    if (m_active_count == 0 || !m_vao) return;
    
    glBindVertexArray(m_vao);
    glDrawArraysInstanced(GL_TRIANGLES, 0, 6, static_cast<GLsizei>(m_active_count));
    glBindVertexArray(0);
}

void ParticleEmitter::emit_burst(u32 count) {
    for (u32 i = 0; i < count; ++i) {
        emit_particle();
    }
}

void ParticleEmitter::emit_particle() {
    // Find inactive particle slot
    for (auto& p : m_particles) {
        if (p.active) continue;
        
        // Initialize particle
        p.active = true;
        
        // Position with variance
        p.position = m_config.position + random_vec3(m_config.position_variance);
        
        // Velocity with variance
        p.velocity = m_config.velocity + random_vec3(m_config.velocity_variance);
        
        // Colors
        p.color = m_config.color_start;
        p.color_end = m_config.color_end;
        
        // Size
        p.size = m_config.size_start;
        p.size_end = m_config.size_end;
        
        // Rotation
        p.rotation = random_range(0.0f, glm::two_pi<float>());
        p.rotation_speed = m_config.rotation_speed + 
                          random_range(-m_config.rotation_variance, m_config.rotation_variance);
        
        // Lifetime
        p.max_life = random_range(m_config.life_min, m_config.life_max);
        p.life = 1.0f;
        
        return; // Only emit one particle
    }
}

float ParticleEmitter::random_range(float min, float max) {
    std::uniform_real_distribution<float> dist(min, max);
    return dist(m_rng);
}

glm::vec3 ParticleEmitter::random_vec3(const glm::vec3& variance) {
    return glm::vec3(
        random_range(-variance.x, variance.x),
        random_range(-variance.y, variance.y),
        random_range(-variance.z, variance.z)
    );
}

// ============================================================================
// ParticleSystem
// ============================================================================

u32 ParticleSystem::create_emitter(const ParticleEmitterConfig& config) {
    auto emitter = std::make_unique<ParticleEmitter>();
    emitter->init(config);
    
    u32 id = static_cast<u32>(m_emitters.size());
    m_emitters.push_back(std::move(emitter));
    
    return id;
}

ParticleEmitter* ParticleSystem::get_emitter(u32 id) {
    if (id >= m_emitters.size()) return nullptr;
    return m_emitters[id].get();
}

void ParticleSystem::remove_emitter(u32 id) {
    if (id >= m_emitters.size()) return;
    m_emitters[id].reset();
}

void ParticleSystem::update(float dt) {
    for (auto& emitter : m_emitters) {
        if (emitter) {
            emitter->update(dt);
        }
    }
}

void ParticleSystem::draw() const {
    for (const auto& emitter : m_emitters) {
        if (emitter) {
            emitter->draw();
        }
    }
}

} // namespace hz
