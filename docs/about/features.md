# Horizon Engine Features

## Core Technology

### Entity-Component-System (ECS)

A pure, Data-Oriented ECS implementation powered by [EnTT](https://github.com/skypjack/entt).

- **Cache-Friendly**: Components are stored in contiguous arrays for optimal CPU cache utilization.
- **Archetype-Based**: Fast iteration over entities with specific component sets.
- **Generational Indices**: Entity handles include a generation counter to prevent use-after-free bugs.

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

Our renderer is built on **OpenGL 4.1 Core Profile** (macOS compatible).

- **Deferred Rendering**: G-Buffer based rendering pipeline with PBR materials.
- **Physically Based Rendering (PBR)**: Cook-Torrance BRDF with metallic-roughness workflow.
- **Image-Based Lighting (IBL)**: HDR environment maps with irradiance and prefiltered convolution.
- **Cascaded Shadow Maps (CSM)**: High-quality directional shadows.
- **Screen-Space Ambient Occlusion (SSAO)**: Real-time ambient occlusion.
- **Temporal Anti-Aliasing (TAA)**: Smooth edges with temporal accumulation.
- **ImGui Integration**: Immediate mode GUI for engine tools and debug overlays.

---

## Gameplay Systems

### Input

- **Action Mapping**: Map physical keys to logical actions (`Jump`, `Fire`).
- **Contexts**: Switch input sets dynamically (e.g., `Walking` vs `Driving`).
- **Raw Input**: Low-latency mouse and keyboard reading via GLFW 3.4.

### Physics

Fully integrated **Jolt Physics** engine.

- Deterministic rigid body simulation.
- Box, sphere, and capsule colliders.
- Character controllers for FPS movement.
- Raycasting for hit detection.

### Animation

- Skeletal animation with bone hierarchies.
- Animation blending and state machines.
- Two-bone IK solver for procedural animation.

### Audio

- 3D spatial audio via miniaudio.
- Sound effects and ambient audio support.
