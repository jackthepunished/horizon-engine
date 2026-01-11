# Rendering and Shader System

## Shader Preprocessor

The Horizon Engine includes a custom **Shader Preprocessor** that runs at runtime before shader compilation. This allows for modular GLSL code and reuse of common functions.

### Features

- **`#include` Support**: Shaders can include other GLSL files using recursive `#include` directives.
- **Path Resolution**: Includes are resolved relative to `assets/shaders/`.
- **Anti-Recursion**: Prevents circular includes and multiple inclusion of the same file (pragma once behavior).

### Usage

```glsl
// assets/shaders/my_shader.frag
#version 410 core

// Include common libraries
#include "common/lights.glsl"
#include "common/pbr_functions.glsl"

out vec4 FragColor;

void main() {
    // Use functions and structs from included files
    vec3 result = PBR_CalculateLighting(...);
    FragColor = vec4(result, 1.0);
}
```

## Common GLSL Library

Located in `assets/shaders/common/`, the library provides standard structures and functions:

- **`common/math.glsl`**: Constants (`PI`, `EPSILON`) and utility math functions.
- **`common/lights.glsl`**: Standard `DirectionalLight` and `PointLight` structs and uniform definitions.
- **`common/pbr_functions.glsl`**: PBR lighting calculations (GGX, Fresnel, Geometry Smith).
- **`common/fog.glsl`**: Atmospheric fog implementation.

## Render Features

- **PBR Pipeline**: Physically Based Rendering using the Cook-Torrance BRDF.
- **Vegetation**: Instanced rendering for grass with wind animation and LOD handling.
- **Terrain**: Multi-textured terrain blending with height-based mixing.
