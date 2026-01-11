# Horizon Engine

[![C++20](https://img.shields.io/badge/C++-20-blue.svg)](https://isocpp.org/)
[![OpenGL](https://img.shields.io/badge/OpenGL-4.1-green.svg)](https://www.opengl.org/)
[![License](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Tests](https://img.shields.io/badge/Tests-7%20passing-brightgreen.svg)](#testing)

A modern **C++20 game engine** built for FPS games, featuring an Entity-Component-System architecture, PBR rendering pipeline, and action-based input system.

I'm learning OpenGL, so this is a work in progress. Planning to migrate (or from zero)to Vulkan in the future.
I'm using this project to learn and experiment with new technologies.
Strictly keeping up with learnopengl.com

## Features

### Core Systems

- **Entity-Component-System (ECS)** - Data-oriented architecture for high performance
- **Physics Integration** - Rigid body dynamics using Jolt Physics
- **Audio System** - 3D spatial audio using miniaudio
- **Action-Based Input** - Abstract input mapping for keyboard, mouse, and gamepad
- **Fixed-Timestep Game Loop** - Deterministic physics at 60 Hz
- **Memory Arena Allocator** - Efficient memory management with `std::pmr`

### Rendering

- **OpenGL 4.1 PBR Renderer** - Cross-platform graphics (macOS, Windows, Linux)
- **Shader Preprocessor** - Runtime `#include` support for modular GLSL
- **Common GLSL Library** - Reusable lighting and math functions
- **Vegetation Rendering** - 3D grass with wind animation and billboarding
- **Material System** - First-class PBR materials with texture handles
- **GLTF Model Loading** - Full 3D model support via tinygltf
- **HDR Pipeline** - Bloom, tone mapping, exposure control
- **SSAO** - Screen-space ambient occlusion
- **Shadow Mapping** - Directional light shadows
- **HDRI Skybox** - Equirectangular environment maps

### Tools & Editor

- **Scene Serialization** - JSON-based scene save/load (F5/F6)
- **In-Game Editor** - Entity hierarchy, material inspector, scene settings
- **Debug Overlay** - FPS, frame time, entity count (F3)
- **ImGui Interface** - Full debug tooling

### Asset Management

- **Handle-Based Registry** - Textures, Models, Materials, Sounds
- **Automatic Caching** - Prevents duplicate asset loads
- **Hot Reload Ready** - Asset reloading infrastructure

## Quick Start

### Prerequisites

- CMake 3.21+
- C++20 compiler (Clang 14+, GCC 11+, MSVC 2022+)
- Git

### Build

```bash
# Clone
git clone https://github.com/jackthepunished/horizon-engine.git
cd horizon-engine

# Configure & Build
cmake -B build -DCMAKE_BUILD_TYPE=Release
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
| `Space`   | Jump/Up       |
| `Ctrl`    | Crouch/Down   |
| `Tab`     | Toggle cursor |
| `F3`      | Debug overlay |
| `F5`      | Save scene    |
| `F6`      | Load scene    |
| `Esc`     | Quit/Menu     |

## Architecture

```
horizon-engine/
├── engine/               # Core engine library
│   ├── assets/           # Textures, Models, Materials, Cubemaps
│   ├── audio/            # Spatial audio system
│   ├── core/             # Logging, memory, game loop
│   ├── ecs/              # Entity-Component-System
│   ├── physics/          # Jolt Physics integration
│   ├── platform/         # Window, input abstraction
│   ├── renderer/         # OpenGL renderer, shaders, framebuffers
│   ├── scene/            # Components, serialization
│   └── ui/               # ImGui integration
├── game/                 # Sample FPS game
│   └── src/              # Main loop, editor UI
├── assets/               # Shaders, textures, models
├── tests/                # Unit tests (Catch2)
└── third_party/          # External dependencies
```

## Dependencies

All dependencies are fetched automatically via CMake FetchContent:

| Library                                           | Version | Purpose            |
| ------------------------------------------------- | ------- | ------------------ |
| [GLFW](https://www.glfw.org/)                     | 3.4     | Windowing & Input  |
| [GLM](https://github.com/g-truc/glm)              | 1.0.1   | Math Library       |
| [spdlog](https://github.com/gabime/spdlog)        | 1.14.1  | Logging            |
| [Catch2](https://github.com/catchorg/Catch2)      | 3.5.2   | Testing            |
| [Jolt](https://github.com/jrouwe/JoltPhysics)     | 5.0.0   | Physics Engine     |
| [miniaudio](https://miniaud.io/)                  | 0.11.21 | Audio Engine       |
| [Dear ImGui](https://github.com/ocornut/imgui)    | 1.90.4  | UI Library         |
| [stb_image](https://github.com/nothings/stb)      | master  | Image Loading      |
| [tinyobjloader](https://github.com/tinyobjloader) | 2.0.0   | OBJ Model Loading  |
| [tinygltf](https://github.com/syoyo/tinygltf)     | 2.8.x   | GLTF Model Loading |
| [nlohmann/json](https://github.com/nlohmann/json) | 3.11.x  | JSON Parsing       |

## Testing

```bash
cd build && ctest --output-on-failure
```

```
100% tests passed, 0 tests failed out of 7
```

## Roadmap

- [x] **Milestone 0**: Repository & Build System
- [x] **Milestone 1**: Core Systems (ECS, Input, Game Loop)
- [x] **Milestone 2**: OpenGL Rendering (Shaders, Meshes, Camera)
- [x] **Milestone 3**: Assets & Hot Reload
- [x] **Milestone 4**: Physics (Jolt Integration)
- [x] **Milestone 5**: Audio, AI, UI (Dear ImGui)
- [x] **Milestone 6**: PBR Rendering, Materials, GLTF Loading
- [ ] **Milestone 7**: Skeletal Animation & Advanced Lighting
  - Bone hierarchy & animation clips
  - GPU skinning
  - IBL (irradiance, prefiltered environment maps)
  - Cascaded Shadow Maps
- [ ] **Milestone 8**: Render Architecture & Optimization
  - Render pass system (Geometry, Shadow, Lighting, Post)
  - Frustum culling & instanced rendering
  - GLTF material extraction
- [ ] **Milestone 9**: Networking & Multiplayer _(optional)_

## License

MIT License - see [LICENSE](LICENSE) for details.
