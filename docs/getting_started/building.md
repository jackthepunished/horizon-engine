# Building Horizon Engine

## Prerequisites

Horizon Engine targets **C++20** and strictly adheres to modern standards.

### Windows

- **Compiler**: MSVC 2022 (v143) or Clang-CL 14+
- **Tools**: CMake 3.21+, Ninja
- **SDKs**: Vulkan SDK 1.3+

### macOS

- **Compiler**: Apple Clang 14+ (Xcode 14+)
- **Tools**: CMake 3.21+, Ninja
- **SDKs**: Vulkan SDK (via MoltenVK, usually included in the installer)

### Linux

- **Compiler**: GCC 11+ or Clang 14+
- **Tools**: CMake 3.21+, Ninja, Make
- **Packages**: `libx11-dev`, `libwayland-dev`, `libxkbcommon-dev`

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

## Troubleshooting

### "C++20 not supported"

Ensure you are not using an old compiler. Check `g++ --version` or `dxc --version`.

### "Vulkan SDK not found"

The engine requires the `VULKAN_SDK` environment variable to be set. Install the SDK from [LunarG](https://vulkan.lunarg.com/).
