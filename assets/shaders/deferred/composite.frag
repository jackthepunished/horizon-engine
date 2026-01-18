#version 410 core

/**
 * Deferred Composite/Tonemapping Shader
 * 
 * Final pass: HDR to LDR with ACES tonemapping and gamma correction
 */

out vec4 FragColor;

in vec2 v_TexCoord;

uniform sampler2D u_HDRBuffer;
uniform sampler2D u_BloomBuffer;
uniform float u_Exposure;
uniform float u_BloomIntensity;
uniform bool u_BloomEnabled;
uniform int u_TonemapOperator; // 0=ACES, 1=Reinhard, 2=Uncharted2
uniform bool u_FXAAEnabled;

// -----------------------------------------------------------------------------
// FXAA 3.11 Quality - Higher quality edge detection
// -----------------------------------------------------------------------------

float luma(vec3 c) { return dot(c, vec3(0.299, 0.587, 0.114)); }

vec3 fxaaHighQuality(sampler2D tex, vec2 uv) {
    vec2 rcp = 1.0 / vec2(textureSize(tex, 0));
    
    // Sample center and 4 neighbors
    vec3 rgbM  = texture(tex, uv).rgb;
    vec3 rgbN  = texture(tex, uv + vec2(0.0, -1.0) * rcp).rgb;
    vec3 rgbS  = texture(tex, uv + vec2(0.0, 1.0) * rcp).rgb;
    vec3 rgbE  = texture(tex, uv + vec2(1.0, 0.0) * rcp).rgb;
    vec3 rgbW  = texture(tex, uv + vec2(-1.0, 0.0) * rcp).rgb;
    
    // Also sample corners for better edge detection
    vec3 rgbNW = texture(tex, uv + vec2(-1.0, -1.0) * rcp).rgb;
    vec3 rgbNE = texture(tex, uv + vec2(1.0, -1.0) * rcp).rgb;
    vec3 rgbSW = texture(tex, uv + vec2(-1.0, 1.0) * rcp).rgb;
    vec3 rgbSE = texture(tex, uv + vec2(1.0, 1.0) * rcp).rgb;

    float lumaM  = luma(rgbM);
    float lumaN  = luma(rgbN);
    float lumaS  = luma(rgbS);
    float lumaE  = luma(rgbE);
    float lumaW  = luma(rgbW);
    float lumaNW = luma(rgbNW);
    float lumaNE = luma(rgbNE);
    float lumaSW = luma(rgbSW);
    float lumaSE = luma(rgbSE);

    float lumaMin = min(lumaM, min(min(min(lumaN, lumaS), min(lumaE, lumaW)),
                                   min(min(lumaNW, lumaNE), min(lumaSW, lumaSE))));
    float lumaMax = max(lumaM, max(max(max(lumaN, lumaS), max(lumaE, lumaW)),
                                   max(max(lumaNW, lumaNE), max(lumaSW, lumaSE))));
    
    float lumaRange = lumaMax - lumaMin;
    
    // Skip if contrast is too low
    const float FXAA_EDGE_THRESHOLD = 0.125;
    const float FXAA_EDGE_THRESHOLD_MIN = 0.0312;
    if (lumaRange < max(FXAA_EDGE_THRESHOLD_MIN, lumaMax * FXAA_EDGE_THRESHOLD)) {
        return rgbM;
    }

    // Calculate gradient direction
    float edgeHorz = abs((lumaNW + lumaN + lumaNE) - (lumaSW + lumaS + lumaSE));
    float edgeVert = abs((lumaNW + lumaW + lumaSW) - (lumaNE + lumaE + lumaSE));
    bool isHorizontal = edgeHorz >= edgeVert;
    
    // Choose edge endpoints
    float luma1 = isHorizontal ? lumaN : lumaW;
    float luma2 = isHorizontal ? lumaS : lumaE;
    float gradient1 = abs(luma1 - lumaM);
    float gradient2 = abs(luma2 - lumaM);
    bool is1Steeper = gradient1 >= gradient2;
    
    float gradientScaled = 0.25 * max(gradient1, gradient2);
    float stepLength = isHorizontal ? rcp.y : rcp.x;
    
    float lumaLocalAvg;
    if (is1Steeper) {
        stepLength = -stepLength;
        lumaLocalAvg = 0.5 * (luma1 + lumaM);
    } else {
        lumaLocalAvg = 0.5 * (luma2 + lumaM);
    }
    
    vec2 currentUv = uv;
    if (isHorizontal) {
        currentUv.y += stepLength * 0.5;
    } else {
        currentUv.x += stepLength * 0.5;
    }
    
    // Edge search
    vec2 offset = isHorizontal ? vec2(rcp.x, 0.0) : vec2(0.0, rcp.y);
    vec2 uv1 = currentUv - offset;
    vec2 uv2 = currentUv + offset;
    
    float lumaEnd1, lumaEnd2;
    bool done1 = false, done2 = false;
    
    const int FXAA_SEARCH_STEPS = 10;
    for (int i = 0; i < FXAA_SEARCH_STEPS; i++) {
        if (!done1) {
            lumaEnd1 = luma(texture(tex, uv1).rgb) - lumaLocalAvg;
            done1 = abs(lumaEnd1) >= gradientScaled;
        }
        if (!done2) {
            lumaEnd2 = luma(texture(tex, uv2).rgb) - lumaLocalAvg;
            done2 = abs(lumaEnd2) >= gradientScaled;
        }
        if (done1 && done2) break;
        if (!done1) uv1 -= offset;
        if (!done2) uv2 += offset;
    }
    
    float distance1 = isHorizontal ? (uv.x - uv1.x) : (uv.y - uv1.y);
    float distance2 = isHorizontal ? (uv2.x - uv.x) : (uv2.y - uv.y);
    
    bool isDirection1 = distance1 < distance2;
    float distanceFinal = min(distance1, distance2);
    float edgeLength = distance1 + distance2;
    
    float pixelOffset = -distanceFinal / edgeLength + 0.5;
    
    bool isLumaCenterSmaller = lumaM < lumaLocalAvg;
    bool correctVariation = ((isDirection1 ? lumaEnd1 : lumaEnd2) < 0.0) != isLumaCenterSmaller;
    float finalOffset = correctVariation ? pixelOffset : 0.0;
    
    vec2 finalUv = uv;
    if (isHorizontal) {
        finalUv.y += finalOffset * stepLength;
    } else {
        finalUv.x += finalOffset * stepLength;
    }
    
    return texture(tex, finalUv).rgb;
}

// ACES Tonemapping
vec3 tonemapACES(vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

// Reinhard Tonemapping
vec3 tonemapReinhard(vec3 x) {
    return x / (x + vec3(1.0));
}

// Uncharted 2 Tonemapping
vec3 unchartedPartial(vec3 x) {
    float A = 0.15;
    float B = 0.50;
    float C = 0.10;
    float D = 0.20;
    float E = 0.02;
    float F = 0.30;
    return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

vec3 tonemapUncharted2(vec3 x) {
    float W = 11.2;
    vec3 curr = unchartedPartial(x);
    vec3 whiteScale = vec3(1.0) / unchartedPartial(vec3(W));
    return curr * whiteScale;
}

void main() {
    // Apply high-quality FXAA if enabled (works well even after TAA)
    vec3 hdrColor = u_FXAAEnabled ? fxaaHighQuality(u_HDRBuffer, v_TexCoord)
                                  : texture(u_HDRBuffer, v_TexCoord).rgb;
    
    // Add bloom
    if (u_BloomEnabled) {
        vec3 bloom = texture(u_BloomBuffer, v_TexCoord).rgb;
        hdrColor += bloom * u_BloomIntensity;
    }
    
    // Apply exposure
    hdrColor *= u_Exposure;
    
    // Tonemap
    vec3 mapped;
    if (u_TonemapOperator == 0) {
        mapped = tonemapACES(hdrColor);
    } else if (u_TonemapOperator == 1) {
        mapped = tonemapReinhard(hdrColor);
    } else {
        mapped = tonemapUncharted2(hdrColor);
    }
    
    // Gamma correction
    mapped = pow(mapped, vec3(1.0 / 2.2));
    
    FragColor = vec4(mapped, 1.0);
}
