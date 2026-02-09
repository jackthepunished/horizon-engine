# Building Horizon Engine

## Prerequisites

Horizon Engine targets **C++20** and uses **Vulkan 1.3** for rendering.

### Windows

- **Compiler**: MSVC 2022 (v143) or Clang-CL 16+
- **Tools**: CMake 3.25+, Ninja
- **SDK**: [Vulkan SDK 1.3.268+](https://vulkan.lunarg.com/)

### macOS

> **Note**: macOS support via MoltenVK is currently experimental.

- **Compiler**: Apple Clang 15+ (Xcode 15+)
- **Tools**: CMake 3.25+, Ninja
- **SDK**: [Vulkan SDK 1.3.268+](https://vulkan.lunarg.com/) (includes MoltenVK)

### Linux

- **Compiler**: GCC 13+ or Clang 16+
- **Tools**: CMake 3.25+, Ninja
- **SDK**: [Vulkan SDK 1.3.268+](https://vulkan.lunarg.com/)
- **Packages**:
  - `libwayland-dev`, `libxkbcommon-dev` (Wayland)
  - `libx11-dev`, `libxrandr-dev`, `libxi-dev` (X11)
  - `libvulkan-dev`, `vulkan-tools`

---

## Build Instructions

We recommend using **Ninja** for the fastest build times.

1. **Clone the Repository**

   ```bash
   git clone https://github.com/jackthepunished/horizon-engine.git
   cd horizon-engine
   ```

2. **Configure (Debug)**

   ```bash
   cmake -B build -DCMAKE_BUILD_TYPE=Debug -GNinja
   ```

3. **Build**

   ```bash
   cmake --build build --parallel
   ```

4. **Run**
   ```bash
   ./build/bin/horizon_game
   ```

---

## Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `HZ_BUILD_TESTS` | `ON` | Build unit tests |
| `WERROR` | `OFF` | Treat warnings as errors |
| `HZ_HEADLESS` | `OFF` | Build without display (for CI) |

Example with options:
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DHZ_BUILD_TESTS=ON -DWERROR=ON -GNinja
```

---

## Troubleshooting

### "Vulkan SDK not found"

Ensure the `VULKAN_SDK` environment variable is set. On Windows, the installer usually does this. On Linux/macOS, source the setup script:
```bash
source ~/VulkanSDK/1.3.xx.x/setup-env.sh
```

### "C++20 not supported"

Ensure you are using a modern compiler. Check `g++ --version` or `clang++ --version`.
