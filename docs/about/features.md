# Horizon Engine Features

## Core Technology

### Entity-Component-System (ECS)

A pure, Data-Oriented ECS implementation designed for massive scale.

- **Cache-Friendly**: Components are stored in contiguous arrays (`std::pmr::vector`).
- **Archetype-Based**: Fast iteration over entities with specific component sets.
- **Generational Indices**: `EntityHandle` includes a generation counter to prevent use-after-free bugs.

### Memory Management

Zero-allocation runtime using `std::pmr` polymorphic allocators.

- **Frame Arena**: A linear allocator that resets every frame for temporary data.
- **System Pools**: Dedicated pools for long-lived subsystem data.
- **Leak Detection**: Debug builds track all allocations and report leaks on shutdown.

### C++20 Foundation

- **Concepts**: Interfaces are enforced at compile-time.
- **Modules**: Faster compilation and better encapsulation (roadmap).
- **Coroutines**: Asynchronous asset loading and script-like sequencing (roadmap).

---

## Graphics & Rendering

Our renderer is built on **OpenGL 4.6 Core Profile**.

- **Bindless Textures**: (Planned) access thousands of textures without binding slots.
- **Direct State Access (DSA)**: Modern OpenGL state management.
- **SPIR-V Shaders**: Cross-API shader compilation pipeline.
- **ImGui Integration**: Immediate mode GUI for engine tools and debug overlays.

---

## Gameplay Systems

### Input

- **Action Mapping**: Map physical keys to logical actions (`Jump`, `Fire`).
- **Contexts**: Switch input sets dynamically (e.g., `Walking` vs `Driving`).
- **Raw Input**: Low-latency mouse and keyboard reading via GLFW 3.4.

### Physics (Coming Soon)

- Integration with **Jolt Physics**.
- Deterministic rigid body simulation.
- Character controllers for FPS movement.
