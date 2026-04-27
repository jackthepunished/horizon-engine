#pragma once

/**
 * @file mesh.hpp
 * @brief Basic mesh class with RHI GPU buffer management
 */

#include "engine/core/types.hpp"
#include "engine/rhi/rhi_command_list.hpp"
#include "engine/rhi/rhi_device.hpp"
#include "engine/rhi/rhi_resources.hpp"

#include <memory>
#include <vector>

#include <glm/glm.hpp>

namespace hz {

// Maximum bones influencing a single vertex (must match shader)
constexpr int MAX_BONE_INFLUENCE = 4;

/**
 * @brief Vertex structure with skeletal animation support
 */
struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texcoord;
    glm::vec4 tangent; // For normal mapping (TBN matrix) + Handedness

    // Skeletal animation data
    int bone_ids[MAX_BONE_INFLUENCE] = {-1, -1, -1, -1};
    float bone_weights[MAX_BONE_INFLUENCE] = {0.0f, 0.0f, 0.0f, 0.0f};

    /**
     * @brief Add a bone influence to this vertex
     */
    void add_bone(int bone_id, float weight) {
        for (int i = 0; i < MAX_BONE_INFLUENCE; ++i) {
            if (bone_ids[i] < 0) {
                bone_ids[i] = bone_id;
                bone_weights[i] = weight;
                return;
            }
        }
    }

    /**
     * @brief Reset bone data
     */
    void reset_bones() {
        for (int i = 0; i < MAX_BONE_INFLUENCE; ++i) {
            bone_ids[i] = -1;
            bone_weights[i] = 0.0f;
        }
    }
};

/**
 * @brief Basic mesh class with GPU buffer management
 *
 * CPU vertex/index data is stored for geometry queries. Call upload_to_gpu()
 * after construction to create device-local vertex and index buffers.
 * Then use draw() with a CommandList to issue draw calls.
 */
class Mesh {
public:
    Mesh(std::vector<Vertex> vertices, std::vector<u32> indices);
    ~Mesh() = default;

    HZ_NON_COPYABLE(Mesh);
    HZ_DEFAULT_MOVABLE(Mesh);

    // =========================================================================
    // GPU Resource Management
    // =========================================================================

    /**
     * @brief Upload vertex/index data to GPU-local buffers
     * @param device RHI device to create buffers on
     * @param debug_name Optional name for GPU debuggers
     */
    void upload_to_gpu(rhi::Device& device, const char* debug_name = nullptr);

    /**
     * @brief Check if GPU buffers have been created
     */
    [[nodiscard]] bool is_uploaded() const { return m_vertex_buffer != nullptr; }

    /**
     * @brief Release GPU buffers
     */
    void release_gpu_resources();

    // =========================================================================
    // Drawing
    // =========================================================================

    /**
     * @brief Bind vertex/index buffers and issue an indexed draw call
     * @param cmd Command list to record into
     * @note Requires upload_to_gpu() to have been called first
     */
    void draw(rhi::CommandList& cmd) const;

    /**
     * @brief Bind buffers and draw multiple instances
     * @param cmd Command list to record into
     * @param instance_count Number of instances to draw
     */
    void draw_instanced(rhi::CommandList& cmd, u32 instance_count) const;

    // =========================================================================
    // Primitive Factory Methods
    // =========================================================================

    [[nodiscard]] static Mesh create_plane(f32 size = 20.0f, i32 subdivisions = 10);
    [[nodiscard]] static Mesh create_cube(f32 size = 1.0f);
    [[nodiscard]] static Mesh create_sphere(f32 radius = 1.0f, i32 slices = 32, i32 stacks = 16);

    // =========================================================================
    // Accessors
    // =========================================================================

    [[nodiscard]] const std::vector<Vertex>& vertices() const { return m_vertices; }
    [[nodiscard]] const std::vector<u32>& indices() const { return m_indices; }
    [[nodiscard]] u32 index_count() const { return static_cast<u32>(m_indices.size()); }
    [[nodiscard]] u32 vertex_count() const { return static_cast<u32>(m_vertices.size()); }

    [[nodiscard]] rhi::Buffer* vertex_buffer() const { return m_vertex_buffer.get(); }
    [[nodiscard]] rhi::Buffer* index_buffer() const { return m_index_buffer.get(); }

    // =========================================================================
    // GPU-Driven Rendering
    // =========================================================================

    /// Local-space bounding sphere (xyz=center, w=radius). Computed once on first access.
    [[nodiscard]] glm::vec4 bounding_sphere() const;

    /// Index assigned by GPUScene::register_mesh; UINT32_MAX if unregistered.
    [[nodiscard]] u32 gpu_mesh_index() const { return m_gpu_mesh_index; }
    void set_gpu_mesh_index(u32 idx) const { m_gpu_mesh_index = idx; }

private:
    std::vector<Vertex> m_vertices;
    std::vector<u32> m_indices;

    // GPU buffers (created by upload_to_gpu)
    std::unique_ptr<rhi::Buffer> m_vertex_buffer;
    std::unique_ptr<rhi::Buffer> m_index_buffer;

    // GPU-driven rendering: lazy-cached bounds and registry index.
    mutable glm::vec4 m_bounding_sphere{0.0f};
    mutable bool m_bounds_cached{false};
    mutable u32 m_gpu_mesh_index{UINT32_MAX};
};

} // namespace hz
