/**
 * =============================================================================
 * HORIZON ENGINE - SHADER LIBRARY v2.0
 * =============================================================================
 * 
 * File: core/math.glsl
 * Purpose: Mathematical utility functions
 * =============================================================================
 */

#ifndef HZ_MATH_GLSL
#define HZ_MATH_GLSL

#include "defines.glsl"

// =============================================================================
// BASIC MATH
// =============================================================================

// Square
float sq(float x) { return x * x; }
vec2 sq(vec2 x) { return x * x; }
vec3 sq(vec3 x) { return x * x; }

// Reciprocal
float rcp(float x) { return 1.0 / x; }
vec3 rcp(vec3 x) { return 1.0 / x; }

// Safe reciprocal (avoids division by zero)
float rcpSafe(float x) { return 1.0 / max(x, EPSILON); }
vec3 rcpSafe(vec3 x) { return 1.0 / max(x, vec3(EPSILON)); }

// Power of 2
float pow2(float x) { return x * x; }
float pow3(float x) { return x * x * x; }
float pow4(float x) { float x2 = x * x; return x2 * x2; }
float pow5(float x) { float x2 = x * x; return x2 * x2 * x; }

// =============================================================================
// INTERPOLATION
// =============================================================================

// Smooth step alternatives
float smootherstep(float edge0, float edge1, float x) {
    x = clamp((x - edge0) / (edge1 - edge0), 0.0, 1.0);
    return x * x * x * (x * (x * 6.0 - 15.0) + 10.0);
}

// Bias and gain (Ken Perlin)
float bias(float x, float b) {
    return x / ((1.0 / b - 2.0) * (1.0 - x) + 1.0);
}

float gain(float x, float g) {
    if (x < 0.5)
        return bias(x * 2.0, g) / 2.0;
    else
        return bias(x * 2.0 - 1.0, 1.0 - g) / 2.0 + 0.5;
}

// Exponential interpolation
float expLerp(float a, float b, float t) {
    return a * pow(b / a, t);
}

// =============================================================================
// VECTOR OPERATIONS
// =============================================================================

// Safe normalize (returns zero for zero-length vectors)
vec3 safeNormalize(vec3 v) {
    float len = length(v);
    return len > EPSILON ? v / len : vec3(0.0);
}

// Project vector onto plane
vec3 projectOnPlane(vec3 v, vec3 normal) {
    return v - normal * dot(v, normal);
}

// Orthonormal basis from normal (Frisvad)
void createBasis(vec3 N, out vec3 T, out vec3 B) {
    if (N.z < -0.9999999) {
        T = vec3(0.0, -1.0, 0.0);
        B = vec3(-1.0, 0.0, 0.0);
    } else {
        float a = 1.0 / (1.0 + N.z);
        float b = -N.x * N.y * a;
        T = vec3(1.0 - N.x * N.x * a, b, -N.x);
        B = vec3(b, 1.0 - N.y * N.y * a, -N.y);
    }
}

// Rotate vector around axis (Rodrigues)
vec3 rotateAroundAxis(vec3 v, vec3 axis, float angle) {
    float c = cos(angle);
    float s = sin(angle);
    return v * c + cross(axis, v) * s + axis * dot(axis, v) * (1.0 - c);
}

// =============================================================================
// NORMAL ENCODING/DECODING
// =============================================================================

// Octahedron encoding (2 components -> 3 component normal)
vec2 encodeOctahedron(vec3 n) {
    n /= (abs(n.x) + abs(n.y) + abs(n.z));
    if (n.z < 0.0) {
        n.xy = (1.0 - abs(n.yx)) * vec2(n.x >= 0.0 ? 1.0 : -1.0, n.y >= 0.0 ? 1.0 : -1.0);
    }
    return n.xy * 0.5 + 0.5;
}

vec3 decodeOctahedron(vec2 f) {
    f = f * 2.0 - 1.0;
    vec3 n = vec3(f.x, f.y, 1.0 - abs(f.x) - abs(f.y));
    float t = max(-n.z, 0.0);
    n.x += n.x >= 0.0 ? -t : t;
    n.y += n.y >= 0.0 ? -t : t;
    return normalize(n);
}

// Spheremap transform (legacy, for compatibility)
vec2 encodeSpheremap(vec3 n) {
    return n.xy * inversesqrt(8.0 * n.z + 8.0) + 0.5;
}

vec3 decodeSpheremap(vec2 f) {
    f = f * 4.0 - 2.0;
    float l = dot(f, f);
    float z = 1.0 - l * 0.5;
    return vec3(f * sqrt(1.0 - l * 0.25), z);
}

// =============================================================================
// NOISE FUNCTIONS
// =============================================================================

// Hash functions
float hash11(float p) {
    p = fract(p * 0.1031);
    p *= p + 33.33;
    p *= p + p;
    return fract(p);
}

float hash12(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

vec2 hash22(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * vec3(0.1031, 0.1030, 0.0973));
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.xx + p3.yz) * p3.zy);
}

vec3 hash33(vec3 p3) {
    p3 = fract(p3 * vec3(0.1031, 0.1030, 0.0973));
    p3 += dot(p3, p3.yxz + 33.33);
    return fract((p3.xxy + p3.yxx) * p3.zyx);
}

// Gradient noise (Perlin-like)
float gradientNoise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    
    return mix(
        mix(dot(hash22(i + vec2(0.0, 0.0)) * 2.0 - 1.0, f - vec2(0.0, 0.0)),
            dot(hash22(i + vec2(1.0, 0.0)) * 2.0 - 1.0, f - vec2(1.0, 0.0)), u.x),
        mix(dot(hash22(i + vec2(0.0, 1.0)) * 2.0 - 1.0, f - vec2(0.0, 1.0)),
            dot(hash22(i + vec2(1.0, 1.0)) * 2.0 - 1.0, f - vec2(1.0, 1.0)), u.x),
        u.y
    ) * 0.5 + 0.5;
}

// FBM (Fractal Brownian Motion)
float fbm(vec2 p, int octaves) {
    float value = 0.0;
    float amplitude = 0.5;
    float frequency = 1.0;
    
    for (int i = 0; i < octaves; i++) {
        value += amplitude * gradientNoise(p * frequency);
        frequency *= 2.0;
        amplitude *= 0.5;
    }
    
    return value;
}

// Voronoi distance
float voronoi(vec2 p) {
    vec2 n = floor(p);
    vec2 f = fract(p);
    
    float minDist = 1.0;
    for (int j = -1; j <= 1; j++) {
        for (int i = -1; i <= 1; i++) {
            vec2 g = vec2(float(i), float(j));
            vec2 o = hash22(n + g);
            vec2 r = g + o - f;
            float d = dot(r, r);
            minDist = min(minDist, d);
        }
    }
    
    return sqrt(minDist);
}

// =============================================================================
// SAMPLING PATTERNS
// =============================================================================

// Fibonacci spiral on hemisphere
vec3 fibonacciHemisphere(int index, int count, float offset) {
    float i = float(index) + offset;
    float phi = TWO_PI * i / GOLDEN_RATIO;
    float cosTheta = 1.0 - (2.0 * i + 1.0) / (2.0 * float(count));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
    return vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
}

// Halton sequence (low discrepancy)
float halton(int index, int base) {
    float result = 0.0;
    float f = 1.0 / float(base);
    int i = index;
    while (i > 0) {
        result += f * float(i % base);
        i = i / base;
        f /= float(base);
    }
    return result;
}

vec2 halton23(int index) {
    return vec2(halton(index, 2), halton(index, 3));
}

// Poisson disk samples for PCF (precomputed)
const vec2 poissonDisk[16] = vec2[](
    vec2(-0.94201624, -0.39906216),
    vec2( 0.94558609, -0.76890725),
    vec2(-0.09418410, -0.92938870),
    vec2( 0.34495938,  0.29387760),
    vec2(-0.91588581,  0.45771432),
    vec2(-0.81544232, -0.87912464),
    vec2(-0.38277543,  0.27676845),
    vec2( 0.97484398,  0.75648379),
    vec2( 0.44323325, -0.97511554),
    vec2( 0.53742981, -0.47373420),
    vec2(-0.26496911, -0.41893023),
    vec2( 0.79197514,  0.19090188),
    vec2(-0.24188840,  0.99706507),
    vec2(-0.81409955,  0.91437590),
    vec2( 0.19984126,  0.78641367),
    vec2( 0.14383161, -0.14100790)
);

#endif // HZ_MATH_GLSL
