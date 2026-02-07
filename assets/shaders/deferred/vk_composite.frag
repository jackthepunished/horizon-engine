#version 450

/**
 * Composite Pass - Fragment Shader (Vulkan)
 *
 * Reads HDR lighting result and applies:
 *   1. ACES filmic tone mapping
 *   2. Gamma correction (linear -> sRGB)
 *
 * Outputs final LDR color to the swapchain.
 */

layout(location = 0) in vec2 v_TexCoord;
layout(location = 0) out vec4 out_Color;

// HDR lighting result (set 0, binding 0)
layout(set = 0, binding = 0) uniform sampler2D u_HDRColor;

// Exposure via push constants
layout(push_constant) uniform PushConstants {
    float exposure;
} pc;

// --------------------------------------------------------------------------
// ACES Filmic Tone Mapping (fitted curve by Stephen Hill)
// --------------------------------------------------------------------------
vec3 aces_tonemap(vec3 x) {
    // sRGB => XYZ => D65_2_D60 => AP1 => RRT_SAT
    const mat3 aces_input = mat3(
        0.59719, 0.07600, 0.02840,
        0.35458, 0.90834, 0.13383,
        0.04823, 0.01566, 0.83777
    );
    // ODT_SAT => XYZ => D60_2_D65 => sRGB
    const mat3 aces_output = mat3(
         1.60475, -0.10208, -0.00327,
        -0.53108,  1.10813, -0.07276,
        -0.07367, -0.00605,  1.07602
    );

    x = aces_input * x;

    // RRT and ODT fit
    vec3 a = x * (x + 0.0245786) - 0.000090537;
    vec3 b = x * (0.983729 * x + 0.4329510) + 0.238081;
    x = a / b;

    x = aces_output * x;

    return clamp(x, 0.0, 1.0);
}

// --------------------------------------------------------------------------
// Linear to sRGB gamma correction
// --------------------------------------------------------------------------
vec3 linear_to_srgb(vec3 color) {
    vec3 lo = color * 12.92;
    vec3 hi = 1.055 * pow(color, vec3(1.0 / 2.4)) - 0.055;
    return mix(lo, hi, step(vec3(0.0031308), color));
}

void main() {
    vec3 hdr = texture(u_HDRColor, v_TexCoord).rgb;

    // Apply exposure
    hdr *= pc.exposure;

    // Tone map
    vec3 mapped = aces_tonemap(hdr);

    // Gamma correction
    vec3 final_color = linear_to_srgb(mapped);

    out_Color = vec4(final_color, 1.0);
}
