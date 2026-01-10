# Horizon Engine

[![C++20](https://img.shields.io/badge/C++-20-blue.svg)](https://isocpp.org/)
[![OpenGL](https://img.shields.io/badge/OpenGL-4.1-green.svg)](https://www.opengl.org/)
[![License](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Tests](https://img.shields.io/badge/Tests-7%20passing-brightgreen.svg)](#testing)

A modern **C++20 game engine** built for FPS games, featuring an Entity-Component-System architecture, OpenGL 4.1 renderer, and action-based input system.

## Features

- **Entity-Component-System (ECS)** - Data-oriented architecture for high performance
- **OpenGL 4.1 Renderer** - Cross-platform graphics (macOS, Windows, Linux)
- **Physics Integration** - Rigid body dynamics using Jolt Physics
- **Audio System** - 3D spatial audio using miniaudio
- **ImGui Interface** - Debug tooling and overlays
- **Asset Management** - Centralized handle-based asset system (Textures, Models, Sounds)
- **Action-Based Input** - Abstract input mapping for keyboard, mouse, and gamepad
- **Fixed-Timestep Game Loop** - Deterministic physics at 60 Hz
- **Memory Arena Allocator** - Efficient memory management with `std::pmr`
- **FPS Camera** - Smooth mouse look with configurable sensitivity

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

| Key       | Action      |
| --------- | ----------- |
| `W/A/S/D` | Move        |
| `Mouse`   | Look around |
| `Shift`   | Sprint      |
| `Space`   | Jump/Up     |
| `Ctrl`    | Crouch/Down |
| `F3`      | Debug UI    |
| `Esc`     | Quit/Menu   |

## Architecture

```
horizon-engine/
├── engine/           # Core engine library
│   ├── core/         # Logging, memory, game loop
│   ├── ecs/          # Entity-Component-System
│   ├── platform/     # Window, input abstraction
│   └── renderer/     # OpenGL renderer, shaders
├── game/             # Sample FPS game
├── tests/            # Unit tests (Catch2)
└── third_party/      # External dependencies
```

## Dependencies

All dependencies are fetched automatically via CMake FetchContent:

| Library                                        | Version | Purpose           |
| ---------------------------------------------- | ------- | ----------------- |
| [GLFW](https://www.glfw.org/)                  | 3.4     | Windowing & Input |
| [GLM](https://github.com/g-truc/glm)           | 1.0.1   | Math Library      |
| [spdlog](https://github.com/gabime/spdlog)     | 1.14.1  | Logging           |
| [Catch2](https://github.com/catchorg/Catch2)   | 3.5.2   | Testing           |
| [Jolt](https://github.com/jrouwe/JoltPhysics)  | 5.0.0   | Physics Engine    |
| [miniaudio](https://miniaud.io/)               | 0.11.21 | Audio Engine      |
| [Dear ImGui](https://github.com/ocornut/imgui) | 1.90.4  | UI Library        |
| [stb_image](https://github.com/nothings/stb)   | master  | Image Loading     |
| [tinyobj](https://github.com/tinyobjloader)    | 2.0.0   | Model Loading     |

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

## License

MIT License - see [LICENSE](LICENSE) for details.
