#include "gpu_scene.hpp"

#include "engine/core/log.hpp"
#include "engine/renderer/mesh.hpp"

#include <glm/gtc/matrix_inverse.hpp>

namespace hz {

// ============================================================================
// Constructor / Destructor
// ============================================================================

GPUScene::GPUScene(rhi::Device& device, const GPUSceneConfig& config)
    : m_device(device), m_config(config) {
    m_objects.reserve(config.max_objects);
    m_mesh_infos.reserve(config.max_meshes);
    create_buffers();

    HZ_LOG_INFO("GPUScene created: max_objects={}, max_meshes={}, max_vertices={}, max_indices={}",
                config.max_objects, config.max_meshes, config.max_vertices, config.max_indices);
}

GPUScene::~GPUScene() = default;

// ============================================================================
// Buffer Creation
// ============================================================================

void GPUScene::create_buffers() {
    // Object data buffer (dynamic, updated every frame)
    m_object_buffer = m_device.create_buffer(
        {.size = m_config.max_objects * sizeof(GPUObjectData),
         .usage = rhi::BufferUsage::StorageBuffer | rhi::BufferUsage::TransferDst,
         .memory = rhi::MemoryUsage::CPU_To_GPU, // Host-visible for easy updates
         .debug_name = "GPUScene_ObjectBuffer"});

    // Mesh info buffer (static, updated when meshes are registered)
    m_mesh_info_buffer = m_device.create_buffer(
        {.size = m_config.max_meshes * sizeof(GPUMeshInfo),
         .usage = rhi::BufferUsage::StorageBuffer | rhi::BufferUsage::TransferDst,
         .memory = rhi::MemoryUsage::CPU_To_GPU,
         .debug_name = "GPUScene_MeshInfoBuffer"});

    // Draw command buffer (GPU-written by cull shader)
    m_draw_command_buffer = m_device.create_buffer(
        {.size = m_config.max_objects * sizeof(GPUDrawCommand),
         .usage = rhi::BufferUsage::StorageBuffer | rhi::BufferUsage::IndirectBuffer,
         .memory = rhi::MemoryUsage::GPU_Only,
         .debug_name = "GPUScene_DrawCommandBuffer"});

    // Draw count buffer (atomic counter for visible objects)
    m_draw_count_buffer = m_device.create_buffer({.size = sizeof(u32),
                                                  .usage = rhi::BufferUsage::StorageBuffer |
                                                           rhi::BufferUsage::IndirectBuffer |
                                                           rhi::BufferUsage::TransferDst,
                                                  .memory = rhi::MemoryUsage::GPU_Only,
                                                  .debug_name = "GPUScene_DrawCountBuffer"});

    // Vertex mega-buffer (all mesh vertices)
    m_vertex_mega_buffer = m_device.create_buffer(
        {.size = m_config.max_vertices * sizeof(Vertex),
         .usage = rhi::BufferUsage::VertexBuffer | rhi::BufferUsage::TransferDst,
         .memory = rhi::MemoryUsage::GPU_Only,
         .debug_name = "GPUScene_VertexMegaBuffer"});

    // Index mega-buffer (all mesh indices)
    m_index_mega_buffer = m_device.create_buffer(
        {.size = m_config.max_indices * sizeof(u32),
         .usage = rhi::BufferUsage::IndexBuffer | rhi::BufferUsage::TransferDst,
         .memory = rhi::MemoryUsage::GPU_Only,
         .debug_name = "GPUScene_IndexMegaBuffer"});
}

// ============================================================================
// Mesh Registration
// ============================================================================

u32 GPUScene::register_mesh(const Mesh& mesh) {
    // Check if already registered
    auto it = m_mesh_index_map.find(&mesh);
    if (it != m_mesh_index_map.end()) {
        return it->second;
    }

    // Validate capacity
    if (m_mesh_infos.size() >= m_config.max_meshes) {
        HZ_LOG_ERROR("GPUScene: Max mesh count ({}) exceeded", m_config.max_meshes);
        return UINT32_MAX;
    }

    const u32 vertex_count = mesh.vertex_count();
    const u32 index_count = mesh.index_count();

    if (m_vertex_offset + vertex_count > m_config.max_vertices) {
        HZ_LOG_ERROR("GPUScene: Vertex mega-buffer full");
        return UINT32_MAX;
    }
    if (m_index_offset + index_count > m_config.max_indices) {
        HZ_LOG_ERROR("GPUScene: Index mega-buffer full");
        return UINT32_MAX;
    }

    // Create mesh info
    const u32 mesh_index = static_cast<u32>(m_mesh_infos.size());
    GPUMeshInfo info{};
    info.index_count = index_count;
    info.first_index = m_index_offset;
    info.vertex_offset = static_cast<i32>(m_vertex_offset);

    m_mesh_infos.push_back(info);
    m_mesh_index_map[&mesh] = mesh_index;

    // Upload geometry to mega-buffers
    upload_mesh_data(mesh, mesh_index);

    // Update offsets
    m_vertex_offset += vertex_count;
    m_index_offset += index_count;

    HZ_LOG_DEBUG("GPUScene: Registered mesh {} (vertices={}, indices={}, v_offset={}, i_offset={})",
                 mesh_index, vertex_count, index_count, info.vertex_offset, info.first_index);

    return mesh_index;
}

bool GPUScene::is_mesh_registered(const Mesh& mesh) const {
    return m_mesh_index_map.contains(&mesh);
}

u32 GPUScene::get_mesh_index(const Mesh& mesh) const {
    auto it = m_mesh_index_map.find(&mesh);
    return it != m_mesh_index_map.end() ? it->second : UINT32_MAX;
}

void GPUScene::upload_mesh_data(const Mesh& mesh, u32 mesh_index) {
    const GPUMeshInfo& info = m_mesh_infos[mesh_index];

    // Upload vertices: update_buffer(buffer, data, size, offset)
    const auto& vertices = mesh.vertices();
    const u64 vertex_size = vertices.size() * sizeof(Vertex);
    const u64 vertex_byte_offset = static_cast<u64>(info.vertex_offset) * sizeof(Vertex);
    m_device.update_buffer(*m_vertex_mega_buffer, vertices.data(), vertex_size, vertex_byte_offset);

    // Upload indices
    const auto& indices = mesh.indices();
    const u64 index_size = indices.size() * sizeof(u32);
    const u64 index_byte_offset = static_cast<u64>(info.first_index) * sizeof(u32);
    m_device.update_buffer(*m_index_mega_buffer, indices.data(), index_size, index_byte_offset);

    // Upload mesh info
    m_device.update_buffer(*m_mesh_info_buffer, &info, sizeof(GPUMeshInfo),
                           static_cast<u64>(mesh_index) * sizeof(GPUMeshInfo));
}

// ============================================================================
// Per-Frame Object Submission
// ============================================================================

void GPUScene::begin_frame() {
    m_objects.clear();
}

u32 GPUScene::add_object(const glm::mat4& transform, u32 mesh_index, u32 material_index,
                         const glm::vec4& bounding_sphere, GPUObjectFlags flags) {
    if (m_objects.size() >= m_config.max_objects) {
        HZ_LOG_WARN("GPUScene: Max object count exceeded, object dropped");
        return UINT32_MAX;
    }

    GPUObjectData obj{};
    obj.model = transform;
    obj.inverse_transpose_model = glm::inverseTranspose(transform);

    // Transform bounding sphere to world space
    const glm::vec3 center = glm::vec3(
        transform * glm::vec4(bounding_sphere.x, bounding_sphere.y, bounding_sphere.z, 1.0f));
    // Scale radius by maximum scale factor
    const glm::vec3 scale(glm::length(glm::vec3(transform[0])),
                          glm::length(glm::vec3(transform[1])),
                          glm::length(glm::vec3(transform[2])));
    const f32 max_scale = glm::max(scale.x, glm::max(scale.y, scale.z));
    obj.bounding_sphere = glm::vec4(center, bounding_sphere.w * max_scale);

    obj.mesh_index = mesh_index;
    obj.material_index = material_index;
    obj.flags = static_cast<u32>(flags);

    const u32 object_index = static_cast<u32>(m_objects.size());
    m_objects.push_back(obj);

    return object_index;
}

std::unique_ptr<rhi::DescriptorSet>
GPUScene::create_object_descriptor_set(const rhi::DescriptorSetLayout& layout) {
    if (!m_object_set_pool) {
        rhi::DescriptorPoolDesc desc{};
        desc.pool_sizes = {{rhi::DescriptorType::StorageBuffer, 4}};
        desc.max_sets = 4;
        desc.debug_name = "GPUScene_ObjectSetPool";
        m_object_set_pool = m_device.create_descriptor_pool(desc);
    }
    auto set = m_object_set_pool->allocate(layout);
    set->write_storage_buffer(0, *m_object_buffer);
    return set;
}

void GPUScene::end_frame() {
    if (m_objects.empty()) {
        return;
    }

    // Upload object data to GPU: update_buffer(buffer, data, size, offset)
    const u64 size = m_objects.size() * sizeof(GPUObjectData);
    m_device.update_buffer(*m_object_buffer, m_objects.data(), size, 0);
}

} // namespace hz
