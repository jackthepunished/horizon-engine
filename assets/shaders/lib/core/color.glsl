/**
 * =============================================================================
 * HORIZON ENGINE - SHADER LIBRARY v2.0
 * =============================================================================
 * 
 * File: core/color.glsl
 * Purpose: Color space conversions and tone mapping operators
 * =============================================================================
 */

#ifndef HZ_COLOR_GLSL
#define HZ_COLOR_GLSL

#include "defines.glsl"

// =============================================================================
// COLOR SPACE CONVERSIONS
// =============================================================================

// sRGB <-> Linear
vec3 sRGBToLinear(vec3 srgb) {
    return pow(srgb, vec3(2.2));
}

vec3 linearToSRGB(vec3 linear) {
    return pow(linear, vec3(1.0 / 2.2));
}

// Accurate sRGB conversion
vec3 sRGBToLinearAccurate(vec3 srgb) {
    vec3 low = srgb / 12.92;
    vec3 high = pow((srgb + 0.055) / 1.055, vec3(2.4));
    return mix(low, high, step(vec3(0.04045), srgb));
}

vec3 linearToSRGBAccurate(vec3 linear) {
    vec3 low = linear * 12.92;
    vec3 high = 1.055 * pow(linear, vec3(1.0 / 2.4)) - 0.055;
    return mix(low, high, step(vec3(0.0031308), linear));
}

// RGB <-> HSV
vec3 rgbToHsv(vec3 c) {
    vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
    vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));
    
    float d = q.x - min(q.w, q.y);
    float e = 1.0e-10;
    return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

vec3 hsvToRgb(vec3 c) {
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

// RGB <-> XYZ (CIE 1931)
vec3 rgbToXyz(vec3 c) {
    mat3 m = mat3(
        0.4124564, 0.3575761, 0.1804375,
        0.2126729, 0.7151522, 0.0721750,
        0.0193339, 0.1191920, 0.9503041
    );
    return m * c;
}

vec3 xyzToRgb(vec3 c) {
    mat3 m = mat3(
         3.2404542, -1.5371385, -0.4985314,
        -0.9692660,  1.8760108,  0.0415560,
         0.0556434, -0.2040259,  1.0572252
    );
    return m * c;
}

// Luminance
float luminance(vec3 c) {
    return dot(c, vec3(0.2126, 0.7152, 0.0722));
}

float luminanceFast(vec3 c) {
    return dot(c, vec3(0.299, 0.587, 0.114));
}

// =============================================================================
// TONE MAPPING OPERATORS
// =============================================================================

// Reinhard (simple)
vec3 tonemapReinhard(vec3 hdr) {
    return hdr / (hdr + vec3(1.0));
}

// Reinhard extended (with white point)
vec3 tonemapReinhardExtended(vec3 hdr, float whitePoint) {
    float wp2 = whitePoint * whitePoint;
    return (hdr * (1.0 + hdr / wp2)) / (1.0 + hdr);
}

// Reinhard luminance-based
vec3 tonemapReinhardLuminance(vec3 hdr, float whitePoint) {
    float l = luminance(hdr);
    float wp2 = whitePoint * whitePoint;
    float lToned = (l * (1.0 + l / wp2)) / (1.0 + l);
    return hdr * (lToned / l);
}

// Uncharted 2 (John Hable's filmic)
vec3 unchartedPartial(vec3 x) {
    float A = 0.15; // Shoulder Strength
    float B = 0.50; // Linear Strength
    float C = 0.10; // Linear Angle
    float D = 0.20; // Toe Strength
    float E = 0.02; // Toe Numerator
    float F = 0.30; // Toe Denominator
    return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

vec3 tonemapUncharted2(vec3 hdr, float exposure) {
    float W = 11.2; // White point
    vec3 curr = unchartedPartial(hdr * exposure);
    vec3 whiteScale = vec3(1.0) / unchartedPartial(vec3(W));
    return curr * whiteScale;
}

// ACES (Academy Color Encoding System)
vec3 tonemapACES(vec3 hdr) {
    // RRT + ODT approximation by Stephen Hill
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return saturate((hdr * (a * hdr + b)) / (hdr * (c * hdr + d) + e));
}

// ACES fitted (more accurate)
vec3 tonemapACESFitted(vec3 hdr) {
    // sRGB => XYZ => D65_2_D60 => AP1 => RRT_SAT
    mat3 ACESInputMat = mat3(
        0.59719, 0.35458, 0.04823,
        0.07600, 0.90834, 0.01566,
        0.02840, 0.13383, 0.83777
    );
    
    // ODT_SAT => XYZ => D60_2_D65 => sRGB
    mat3 ACESOutputMat = mat3(
         1.60475, -0.53108, -0.07367,
        -0.10208,  1.10813, -0.00605,
        -0.00327, -0.07276,  1.07602
    );
    
    vec3 v = ACESInputMat * hdr;
    
    // RRT/ODT approximation
    vec3 a = v * (v + 0.0245786) - 0.000090537;
    vec3 b = v * (0.983729 * v + 0.4329510) + 0.238081;
    v = a / b;
    
    return ACESOutputMat * v;
}

// Lottes (AMD)
vec3 tonemapLottes(vec3 hdr) {
    const float a = 1.6;  // contrast
    const float d = 0.977;
    const float hdrMax = 8.0;
    const float midIn = 0.18;
    const float midOut = 0.267;
    
    float b = (-pow(midIn, a) + pow(hdrMax, a) * midOut) /
              ((pow(hdrMax, a * d) - pow(midIn, a * d)) * midOut);
    float c = (pow(hdrMax, a * d) * pow(midIn, a) - pow(hdrMax, a) * pow(midIn, a * d) * midOut) /
              ((pow(hdrMax, a * d) - pow(midIn, a * d)) * midOut);
    
    return pow(hdr, vec3(a)) / (pow(hdr, vec3(a * d)) * b + c);
}

// Uchimura (Gran Turismo)
vec3 tonemapUchimura(vec3 hdr, float P, float a, float m, float l, float c, float b) {
    float l0 = ((P - m) * l) / a;
    float L0 = m - m / a;
    float L1 = m + (1.0 - m) / a;
    float S0 = m + l0;
    float S1 = m + a * l0;
    float C2 = (a * P) / (P - S1);
    float CP = -C2 / P;
    
    vec3 w0 = 1.0 - smoothstep(vec3(0.0), vec3(m), hdr);
    vec3 w2 = step(vec3(m + l0), hdr);
    vec3 w1 = 1.0 - w0 - w2;
    
    vec3 T = m * pow(hdr / m, vec3(c)) + b;
    vec3 S = P - (P - S1) * exp(CP * (hdr - S0));
    vec3 L = m + a * (hdr - m);
    
    return T * w0 + L * w1 + S * w2;
}

vec3 tonemapUchimuraDefault(vec3 hdr) {
    const float P = 1.0;   // max display brightness
    const float a = 1.0;   // contrast
    const float m = 0.22;  // linear section start
    const float l = 0.4;   // linear section length
    const float c = 1.33;  // black tightness
    const float b = 0.0;   // pedestal
    return tonemapUchimura(hdr, P, a, m, l, c, b);
}

// Neutral (Unity HDRP-like)
vec3 tonemapNeutral(vec3 hdr) {
    const float startCompression = 0.8 - 0.04;
    const float desaturation = 0.15;
    
    float x = min(hdr.r, min(hdr.g, hdr.b));
    float offset = x < 0.08 ? x - 6.25 * x * x : 0.04;
    hdr -= offset;
    
    float peak = max(hdr.r, max(hdr.g, hdr.b));
    if (peak < startCompression) return hdr;
    
    float d = 1.0 - startCompression;
    float newPeak = 1.0 - d * d / (peak + d - startCompression);
    hdr *= newPeak / peak;
    
    float g = 1.0 - 1.0 / (desaturation * (peak - newPeak) + 1.0);
    return mix(hdr, vec3(newPeak), g);
}

// =============================================================================
// COLOR CORRECTION
// =============================================================================

// Contrast
vec3 applyContrast(vec3 color, float contrast) {
    return (color - 0.5) * contrast + 0.5;
}

// Brightness
vec3 applyBrightness(vec3 color, float brightness) {
    return color + brightness;
}

// Saturation
vec3 applySaturation(vec3 color, float saturation) {
    float lum = luminance(color);
    return mix(vec3(lum), color, saturation);
}

// Vibrance (smart saturation)
vec3 applyVibrance(vec3 color, float vibrance) {
    float lum = luminance(color);
    float mx = max(color.r, max(color.g, color.b));
    float mn = min(color.r, min(color.g, color.b));
    float sat = mx - mn;
    float adjustedVibrance = vibrance * (1.0 - sat);
    return mix(vec3(lum), color, 1.0 + adjustedVibrance);
}

// Color temperature (Kelvin to RGB multiplier)
vec3 colorTemperature(float kelvin) {
    float temp = kelvin / 100.0;
    vec3 color;
    
    if (temp <= 66.0) {
        color.r = 255.0;
        color.g = 99.4708025861 * log(temp) - 161.1195681661;
    } else {
        color.r = 329.698727446 * pow(temp - 60.0, -0.1332047592);
        color.g = 288.1221695283 * pow(temp - 60.0, -0.0755148492);
    }
    
    if (temp >= 66.0) {
        color.b = 255.0;
    } else if (temp <= 19.0) {
        color.b = 0.0;
    } else {
        color.b = 138.5177312231 * log(temp - 10.0) - 305.0447927307;
    }
    
    return clamp(color / 255.0, 0.0, 1.0);
}

// Vignette
float vignette(vec2 uv, float intensity, float smoothness) {
    vec2 d = abs(uv - 0.5) * 2.0;
    float dist = length(d);
    return 1.0 - smoothstep(1.0 - smoothness - intensity, 1.0 - smoothness, dist);
}

#endif // HZ_COLOR_GLSL
