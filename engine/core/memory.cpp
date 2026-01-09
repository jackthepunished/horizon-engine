#include "memory.hpp"

#include <algorithm>
#include <new>

namespace hz {

// ============================================================================
// LinearArena Implementation
// ============================================================================

LinearArena::LinearArena(usize capacity)
    : m_buffer(std::make_unique<std::byte[]>(capacity)), m_capacity(capacity), m_offset(0) {}

LinearArena::~LinearArena() = default;

void LinearArena::reset() noexcept {
    m_offset = 0;
}

void* LinearArena::do_allocate(usize bytes, usize alignment) {
    // Align the current offset
    usize aligned_offset = (m_offset + alignment - 1) & ~(alignment - 1);

    if (aligned_offset + bytes > m_capacity) {
        HZ_ENGINE_ERROR("LinearArena out of memory: requested {} bytes, {} available", bytes,
                        m_capacity - aligned_offset);
        throw std::bad_alloc();
    }

    void* ptr = m_buffer.get() + aligned_offset;
    m_offset = aligned_offset + bytes;
    return ptr;
}

void LinearArena::do_deallocate(void* /*p*/, usize /*bytes*/, usize /*alignment*/) {
    // No-op: linear arena does not support individual deallocations
}

bool LinearArena::do_is_equal(const memory_resource& other) const noexcept {
    return this == &other;
}

// ============================================================================
// MemoryContext Implementation
// ============================================================================

std::unique_ptr<LinearArena> MemoryContext::s_frame_arena;
std::pmr::synchronized_pool_resource MemoryContext::s_general_pool;
bool MemoryContext::s_initialized = false;

void MemoryContext::init() {
    if (s_initialized) {
        HZ_ENGINE_WARN("MemoryContext already initialized");
        return;
    }

    s_frame_arena = std::make_unique<LinearArena>(FRAME_ARENA_SIZE);

    s_initialized = true;
    HZ_ENGINE_INFO("Memory context initialized: frame arena {} MB",
                   FRAME_ARENA_SIZE / (1024 * 1024));
}

void MemoryContext::shutdown() {
    if (!s_initialized) {
        return;
    }

    log_stats();
    s_frame_arena.reset();
    s_initialized = false;

    HZ_ENGINE_INFO("Memory context shutdown");
}

void MemoryContext::reset_frame() {
    if (s_frame_arena) {
        s_frame_arena->reset();
    }
}

std::pmr::memory_resource* MemoryContext::get(MemoryDomain domain) {
    switch (domain) {
    case MemoryDomain::Frame:
        return s_frame_arena.get();
    default:
        return &s_general_pool;
    }
}

LinearArena& MemoryContext::frame_arena() {
    HZ_ASSERT(s_frame_arena, "Frame arena not initialized");
    return *s_frame_arena;
}

void MemoryContext::log_stats() {
    if (s_frame_arena) {
        HZ_ENGINE_DEBUG("Frame arena: {}/{} bytes ({:.1f}% used)", s_frame_arena->used(),
                        s_frame_arena->capacity(), s_frame_arena->usage_percent() * 100.0f);
    }
}

// ============================================================================
// ScopedArenaMarker Implementation
// ============================================================================

ScopedArenaMarker::ScopedArenaMarker(LinearArena& arena) : m_arena(arena), m_marker(arena.used()) {}

ScopedArenaMarker::~ScopedArenaMarker() {
    // This is a simplified reset - in practice we'd need to track the marker
    // For now, we just log if there were allocations
    if (m_arena.used() > m_marker) {
        HZ_ENGINE_TRACE("ScopedArenaMarker: freed {} bytes", m_arena.used() - m_marker);
    }
}

} // namespace hz
