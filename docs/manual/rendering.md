# Rendering Pipeline

The Horizon Engine features a high-performance **Deferred Renderer** built on top of a Vulkan RHI.

## Pipeline Architecture

The rendering process is split into several distinct passes:

1. **Geometry Pass**: Renders opaque objects into the **G-Buffer**.
2. **Decal Pass**: (Planned) Projects decals onto the G-Buffer.
3. **Shadow Pass**: Renders scene depth from light perspectives into **Cascaded Shadow Maps**.
4. **Lighting Pass**: Calculates lighting for every pixel using G-Buffer data.
5. **Transparency Pass**: Forward rendering for transparent objects.
6. **Post-Processing**: Applies screen-space effects.

## G-Buffer Layout

To optimize bandwidth, the G-Buffer is packed into 4 render targets:

| Target | Format | Channels | Data |
|--------|--------|----------|------|
| **RT0** | `RGBA16F` | RGB | Albedo Color |
| | | A | Metallic |
| **RT1** | `RGBA16F` | RG | Octahedron-Encoded Normal |
| | | B | Roughness |
| | | A | Ambient Occlusion |
| **RT2** | `RGBA16F` | RGB | Emission Color |
| | | A | Material ID |
| **RT3** | `RG16F` | RG | Motion Vectors (Velocity) |
| **Depth** | `D32F` | D | Depth |

## Lighting System

- **PBR**: Physically Based Rendering using the standard Cook-Torrance BRDF (GGX distribution, Smith geometry, Fresnel-Schlick).
- **Cascaded Shadow Maps (CSM)**: Directional lights use 4 cascades to provide high-resolution shadows near the camera and stable shadows at a distance.
- **IBL**: Image-Based Lighting for realistic ambient reflections.

## Post-Processing

The post-processing stack includes:

- **TAA (Temporal Anti-Aliasing)**: Reduces aliasing and shimmering using history re-projection.
- **SSR (Screen-Space Reflections)**: Ray-marched reflections based on the depth buffer.
- **Bloom**: Energy-conserving bloom to simulate bright light bleeding.
- **Tonemapping**: ACES tonemapping for cinematic color reproduction.

## Shader System

Shaders are written in GLSL and compiled to SPIR-V at runtime using `shaderc`.

### Features
- **Hot Reloading**: Edit shaders while the game is running.
- **Includes**: Support for `#include "path/to/file.glsl"`.
- **Reflection**: Automatic descriptor set layout generation (Planned).
