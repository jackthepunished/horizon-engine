# Horizon Engine

[![C++20](https://img.shields.io/badge/C++-20-blue.svg)](https://isocpp.org/)
[![Vulkan](https://img.shields.io/badge/Vulkan-1.3-red.svg)](https://www.khronos.org/vulkan/)
[![License](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Tests](https://img.shields.io/badge/Tests-Passing-brightgreen.svg)](#testing)

A modern **C++20 game engine** built for high-performance graphics and gameplay, featuring a **Vulkan Deferred Renderer**, Entity-Component-System architecture, and Jolt Physics.

I'm building this engine to learn and experiment with modern rendering architecture (Vulkan, Render Graphs, GPU-driven rendering) and engine design patterns.

> **Note**: This project is currently in active development. The renderer has been recently migrated from OpenGL to **Vulkan**.

### News (2026-02-10)

**⟳ Vulkan Deferred Renderer**: The rendering backend has been completely rewritten using Vulkan 1.3. It now features a state-of-the-art Deferred Rendering pipeline with G-Buffer, Cascaded Shadow Maps, and TAA.

## Features

### Core Systems

- **Entity-Component-System (ECS)** - Powered by [EnTT](https://github.com/skypjack/entt) for maximum performance and flexibility.
- **Physics Integration** - Rigid body dynamics and character controllers using [Jolt Physics](https://github.com/jrouwe/JoltPhysics).
- **Audio System** - 3D spatial audio using miniaudio.
- **Action-Based Input** - Abstract input mapping for keyboard, mouse, and gamepad.
- **Memory Management** - Custom memory arenas and allocators.

### Rendering (Vulkan)

- **Deferred Rendering Pipeline** - Optimized G-Buffer layout (Albedo, Normal, ARM, Emission, Velocity).
- **Vulkan RHI** - Modern Render Hardware Interface abstracting Vulkan complexity.
- **PBR Lighting** - Physically Based Rendering with image-based lighting (IBL).
- **Shadows** - Cascaded Shadow Maps (CSM) with PCF filtering.
- **Global Illumination** - (Planned) Ray-traced or probe-based GI.
- **Post-Processing**
  - **TAA** - Temporal Anti-Aliasing with jittering.
  - **SSR** - Screen-Space Reflections.
  - **SSAO** - Screen-Space Ambient Occlusion.
  - **Bloom** - High-quality bloom with downsampling/upsampling.
  - **Tonemapping** - ACES / Filmic tonemapping.
- **Asset Pipeline** - Asynchronous asset loading and processing (glTF models, KTX textures).

### Tools

- **ImGui Editor** - In-game editor for scene manipulation, entity inspection, and performance profiling.
- **Scene Serialization** - JSON-based scene saving/loading.
- **Hot Reloading** - Shader re-compilation and asset reloading at runtime.

## Quick Start

### Prerequisites

- **CMake 3.25+**
- **C++20 Compiler** (MSVC 2022, Clang 16+, GCC 13+)
- **Vulkan SDK 1.3+**
- **Git**

### Build

```bash
# Clone the repository
git clone https://github.com/jackthepunished/horizon-engine.git
cd horizon-engine

# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build --parallel

# Run
./build/bin/horizon_game
```

### Controls

| Key       | Action        |
| --------- | ------------- |
| `W/A/S/D` | Move          |
| `Mouse`   | Look around   |
| `Shift`   | Sprint        |
| `Space`   | Jump          |
| `Ctrl`    | Crouch        |
| `Tab`     | Toggle Cursor |
| `F3`      | Debug Stats   |
| `F1`      | Toggle Editor |

## Architecture

```
horizon-engine/
├── engine/               # Core engine library
│   ├── core/             # Logging, memory, events
│   ├── ecs/              # Entity-Component-System registry
│   ├── rhi/              # Render Hardware Interface (Vulkan)
│   ├── renderer/         # Deferred renderer, passes, graphs
│   ├── physics/          # Jolt Physics integration
│   ├── audio/            # Audio system
│   ├── assets/           # Asset manager (Models, Textures, Shaders)
│   ├── scene/            # Scene graph and serialization
│   └── platform/         # OS abstraction (Window, Input)
├── game/                 # Sample Game Application
├── assets/               # Runtime assets (Shaders, Models)
└── tests/                # Unit tests
```

## Dependencies

| Library                                           | Purpose            |
| ------------------------------------------------- | ------------------ |
| [Vulkan](https://www.vulkan.org/)                 | Graphics API       |
| [GLFW](https://www.glfw.org/)                     | Windowing & Input  |
| [GLM](https://github.com/g-truc/glm)              | Math Library       |
| [EnTT](https://github.com/skypjack/entt)          | ECS                |
| [Jolt Physics](https://github.com/jrouwe/JoltPhysics) | Physics        |
| [spdlog](https://github.com/gabime/spdlog)        | Logging            |
| [miniaudio](https://miniaud.io/)                  | Audio              |
| [Dear ImGui](https://github.com/ocornut/imgui)    | UI & Debugging     |
| [tinygltf](https://github.com/syoyo/tinygltf)     | Model Loading      |
| [Catch2](https://github.com/catchorg/Catch2)      | Testing            |

## Roadmap

- [x] **Core**: ECS, Windowing, Input, Logging
- [x] **Physics**: Jolt Integration, Character Controller
- [x] **RHI**: Vulkan Backend (Device, Swapchain, Pipelines, Descriptors)
- [x] **Renderer**: Deferred Pipeline, G-Buffer, PBR
- [x] **Lighting**: Point/Spot/Dir Lights, Cascaded Shadow Maps
- [x] **Post-Process**: Bloom, Tonemapping, TAA, SSR
- [ ] **Advanced Rendering**:
    - GPU-Driven Rendering (Mesh Shaders, Multi-Draw)
    - Ray Tracing (Shadows, Reflections)
    - Volumetric Fog
- [ ] **Editor**:
    - Gizmos
    - Asset Browser
    - Scene Graph Hierarchy

## License

MIT License - see [LICENSE](LICENSE) for details.
