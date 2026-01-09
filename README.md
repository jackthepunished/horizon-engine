# Horizon Engine

A modern **C++20 3D FPS game engine** with Vulkan rendering, designed for performance, safety, determinism, and testability.

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![Vulkan](https://img.shields.io/badge/Vulkan-1.3-red.svg)](https://www.vulkan.org/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

## Features

- **Pure ECS Architecture** - Entities as IDs, components as data, systems as logic
- **RAII Vulkan Wrappers** - No raw `vkDestroy*` calls, automatic cleanup
- **Fixed Timestep Simulation** - Deterministic updates for replays and networking
- **PMR Memory Model** - Custom allocators with frame arenas and pools
- **Action-Based Input** - Configurable bindings for FPS controls
- **Headless Testing** - Run tests without GPU or window

## Building

### Prerequisites

- **CMake 3.21+**
- **C++20 compatible compiler** (Clang 14+, GCC 12+, MSVC 2022+)
- **Vulkan SDK 1.3+**

### Build Commands

```bash
# Clone and build
git clone https://github.com/yourusername/horizon-engine.git
cd horizon-engine

# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Debug

# Build
cmake --build build --parallel

# Run sample game
./build/bin/horizon_game

# Run tests
cd build && ctest --output-on-failure
```

### Build Options

| Option                 | Default | Description                      |
| ---------------------- | ------- | -------------------------------- |
| `HZ_BUILD_TESTS`       | ON      | Build unit tests                 |
| `HZ_BUILD_GAME`        | ON      | Build sample game                |
| `HZ_ENABLE_VALIDATION` | ON      | Enable Vulkan validation layers  |
| `HZ_HEADLESS`          | OFF     | Build without GPU/window support |

## Project Structure

```
horizon-engine/
├── engine/                 # Core engine library
│   ├── core/              # Types, logging, memory, game loop
│   ├── ecs/               # Entity-component-system
│   ├── platform/          # Window, input abstraction
│   └── renderer/          # Vulkan backend
├── game/                   # Sample FPS sandbox
├── tests/                  # Unit tests (Catch2)
├── tools/                  # Asset pipeline, CLI tools
├── third_party/           # Dependencies (FetchContent)
└── docs/                   # Architecture documentation
```

## Architecture Highlights

### RAII Vulkan

Every Vulkan object is wrapped in a move-only RAII type:

```cpp
// Automatic cleanup on destruction
vk::Instance instance{config};
vk::Device device{instance, surface};
vk::Swapchain swapchain{device, surface, width, height};
```

### Pure ECS

```cpp
// Entities are just IDs
Entity player = world.create_entity();

// Components are plain data
world.add_component<Transform>(player, {0, 0, 0});
world.add_component<Health>(player, 100, 100);

// Systems contain all logic
class DamageSystem : public ISystem {
    void update(World& world, f64 dt) override { ... }
};
```

### Fixed Timestep

```cpp
GameLoop loop;
loop.set_update_callback([](f64 dt) {
    // dt is always fixed (e.g., 1/60s)
    world.update(dt);
});
loop.set_render_callback([](f64 alpha) {
    // alpha for interpolation
    renderer.render(alpha);
});
loop.run();
```

## Roadmap

- [x] **Milestone 0**: Repository scaffold + CMake
- [x] **Milestone 1**: Core systems (ECS, input, game loop)
- [x] **Milestone 2**: Vulkan renderer (clear screen)
- [ ] **Milestone 3**: Assets + hot reload
- [ ] **Milestone 4**: FPS controller + physics
- [ ] **Milestone 5**: Audio, AI, UI stubs
- [ ] **Milestone 6**: Networking architecture
- [ ] **Milestone 7**: Tools + packaging

## Dependencies

| Library                                      | Version | Purpose   |
| -------------------------------------------- | ------- | --------- |
| [GLFW](https://www.glfw.org/)                | 3.4     | Windowing |
| [GLM](https://github.com/g-truc/glm)         | 1.0.1   | Math      |
| [spdlog](https://github.com/gabime/spdlog)   | 1.14.1  | Logging   |
| [Vulkan](https://www.vulkan.org/)            | 1.3+    | Graphics  |
| [Catch2](https://github.com/catchorg/Catch2) | 3.5.2   | Testing   |

## License

MIT License - see [LICENSE](LICENSE) for details.
