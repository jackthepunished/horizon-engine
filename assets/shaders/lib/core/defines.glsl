/**
 * =============================================================================
 * HORIZON ENGINE - SHADER LIBRARY v2.0
 * =============================================================================
 * 
 * Modern, modular shader architecture for real-time rendering.
 * 
 * File: core/defines.glsl
 * Purpose: Core definitions, constants, and macros
 * 
 * Include order:
 *   1. core/defines.glsl     - Constants and macros
 *   2. core/uniforms.glsl    - UBO definitions
 *   3. core/functions.glsl   - Math utilities
 *   4. lighting/brdf.glsl    - PBR lighting
 *   5. effects/*.glsl        - Post-processing
 * =============================================================================
 */

#ifndef HZ_DEFINES_GLSL
#define HZ_DEFINES_GLSL

// =============================================================================
// VERSION & COMPATIBILITY
// =============================================================================

#define HZ_SHADER_VERSION 200
#define HZ_GLSL_VERSION 410

// =============================================================================
// MATHEMATICAL CONSTANTS
// =============================================================================

#define PI          3.14159265359
#define TWO_PI      6.28318530718
#define HALF_PI     1.57079632679
#define INV_PI      0.31830988618
#define INV_TWO_PI  0.15915494309
#define SQRT_2      1.41421356237
#define EPSILON     0.0001
#define GOLDEN_RATIO 1.61803398875

// =============================================================================
// RENDERING CONSTANTS
// =============================================================================

// PBR
#define MAX_REFLECTION_LOD  4.0
#define MIN_ROUGHNESS       0.04
#define DIELECTRIC_F0       0.04

// Lighting
#define MAX_POINT_LIGHTS    128
#define MAX_SPOT_LIGHTS     32
#define MAX_CASCADES        4
#define MAX_BONES           128

// Shadow
#define SHADOW_BIAS_MIN     0.0005
#define SHADOW_BIAS_MAX     0.05
#define PCF_RADIUS          2.0
#define PCF_SAMPLES         16

// SSAO
#define SSAO_KERNEL_SIZE    64
#define SSAO_RADIUS         0.5
#define SSAO_BIAS           0.025

// Bloom
#define BLOOM_THRESHOLD     1.0
#define BLOOM_KNEE          0.1

// TAA
#define TAA_FEEDBACK_MIN    0.88
#define TAA_FEEDBACK_MAX    0.97

// =============================================================================
// UTILITY MACROS
// =============================================================================

#define saturate(x)         clamp(x, 0.0, 1.0)
#define sq(x)               ((x) * (x))
#define rcp(x)              (1.0 / (x))
#define linearstep(a, b, x) saturate(((x) - (a)) / ((b) - (a)))

// Swizzle shortcuts
#define xyz3(v)             vec3(v)
#define rgb3(v)             vec3(v)

// Color space
#define sRGB_TO_LINEAR(c)   pow(c, vec3(2.2))
#define LINEAR_TO_sRGB(c)   pow(c, vec3(1.0 / 2.2))

// Luminance (Rec. 709)
#define luminance(c)        dot(c, vec3(0.2126, 0.7152, 0.0722))

// Depth
#define linearize_depth(d, n, f) ((2.0 * n) / (f + n - d * (f - n)))

// Normal encoding/decoding
#define encodeNormal(n)     (n * 0.5 + 0.5)
#define decodeNormal(n)     (n * 2.0 - 1.0)

// =============================================================================
// FEATURE FLAGS (set by preprocessor)
// =============================================================================

// #define HZ_FEATURE_IBL
// #define HZ_FEATURE_SSAO
// #define HZ_FEATURE_SHADOWS
// #define HZ_FEATURE_FOG
// #define HZ_FEATURE_NORMAL_MAP
// #define HZ_FEATURE_EMISSIVE
// #define HZ_FEATURE_PARALLAX
// #define HZ_FEATURE_SKINNING

#endif // HZ_DEFINES_GLSL
