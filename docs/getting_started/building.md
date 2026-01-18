# Building Horizon Engine

## Prerequisites

Horizon Engine targets **C++20** and strictly adheres to modern standards.

### Windows

- **Compiler**: MSVC 2022 (v143) or Clang-CL 14+
- **Tools**: CMake 3.21+, Ninja

### macOS

- **Compiler**: Apple Clang 14+ (Xcode 14+)
- **Tools**: CMake 3.21+, Ninja

### Linux

- **Compiler**: GCC 11+ or Clang 14+
- **Tools**: CMake 3.21+, Ninja, Make
- **Packages**: `libx11-dev`, `libwayland-dev`, `libxkbcommon-dev`, `libxcursor-dev`, `libxinerama-dev`, `libxrandr-dev`, `libxi-dev`

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

### "C++20 not supported"

Ensure you are not using an old compiler. Check `g++ --version` or `clang++ --version`.

### "OpenGL context creation failed"

- **macOS**: Ensure you have a display available. For headless builds, use `-DHZ_HEADLESS=ON`.
- **Linux**: Install Mesa or proprietary GPU drivers.
