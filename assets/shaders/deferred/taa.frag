#version 410 core

/**
 * Temporal Anti-Aliasing (TAA) resolve shader
 * Uses variance clipping for better ghosting rejection and sharpness
 */

out vec4 FragColor;
in vec2 v_TexCoord;

uniform sampler2D u_Current;
uniform sampler2D u_History;
uniform sampler2D u_Velocity;
uniform sampler2D u_Depth;
uniform float u_FeedbackMin;
uniform float u_FeedbackMax;

// Convert RGB to YCoCg for better color clamping
vec3 RGB_to_YCoCg(vec3 rgb) {
    return vec3(
        0.25 * rgb.r + 0.5 * rgb.g + 0.25 * rgb.b,
        0.5 * rgb.r - 0.5 * rgb.b,
        -0.25 * rgb.r + 0.5 * rgb.g - 0.25 * rgb.b
    );
}

vec3 YCoCg_to_RGB(vec3 ycocg) {
    return vec3(
        ycocg.x + ycocg.y - ycocg.z,
        ycocg.x + ycocg.z,
        ycocg.x - ycocg.y - ycocg.z
    );
}

vec3 sampleCurrent(vec2 uv) {
    return texture(u_Current, uv).rgb;
}

// Variance clipping - more accurate than min/max clamping
vec3 clipToAABB(vec3 history, vec3 minColor, vec3 maxColor, vec3 avg) {
    vec3 center = 0.5 * (maxColor + minColor);
    vec3 extents = 0.5 * (maxColor - minColor);
    
    vec3 offset = history - center;
    vec3 v = offset / max(extents, vec3(0.0001));
    
    float maxComp = max(max(abs(v.x), abs(v.y)), abs(v.z));
    if (maxComp > 1.0) {
        return center + offset / maxComp;
    }
    return history;
}

void main() {
    vec2 texel = 1.0 / vec2(textureSize(u_Current, 0));

    // Sample current frame with 3x3 neighborhood
    vec3 curr = sampleCurrent(v_TexCoord);
    
    // Velocity Dilation: Find the closest fragment in 3x3 neighborhood
    // This helps with silhouette anti-aliasing
    vec2 bestVelocity = texture(u_Velocity, v_TexCoord).rg;
    float closestDepth = texture(u_Depth, v_TexCoord).r;
    
    for(int x=-1; x<=1; ++x) {
        for(int y=-1; y<=1; ++y) {
            if(x==0 && y==0) continue;
            vec2 uv = v_TexCoord + vec2(x, y) * texel;
            float d = texture(u_Depth, uv).r;
            if(d < closestDepth) { // Standard GL depth: 0=near, 1=far? Or Reverse-Z?
                // Assuming 0 is near plane (standard)
                closestDepth = d;
                bestVelocity = texture(u_Velocity, uv).rg;
            }
        }
    }

    vec2 velocity = bestVelocity;
    vec2 prevUV = v_TexCoord - velocity;
    
    vec3 hist = texture(u_History, prevUV).rgb;
    
    // Convert to YCoCg for perceptually better clamping
    vec3 currYCoCg = RGB_to_YCoCg(curr);
    vec3 histYCoCg = RGB_to_YCoCg(hist);

    // Gather neighborhood statistics
    vec3 m1 = currYCoCg;  // Sum
    vec3 m2 = currYCoCg * currYCoCg;  // Sum of squares
    vec3 mn = currYCoCg;
    vec3 mx = currYCoCg;
    
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            if (x == 0 && y == 0) continue;
            vec3 c = RGB_to_YCoCg(sampleCurrent(v_TexCoord + vec2(x, y) * texel));
            m1 += c;
            m2 += c * c;
            mn = min(mn, c);
            mx = max(mx, c);
        }
    }
    
    // Calculate mean and variance
    vec3 avg = m1 / 9.0;
    vec3 variance = sqrt(max(m2 / 9.0 - avg * avg, vec3(0.0)));
    
    // Variance clipping (tighter than min/max, less ghosting)
    float gamma = 2.25;  // Relaxed from 1.25 to prevent flickering/shaking on edges
    vec3 minClip = avg - gamma * variance;
    vec3 maxClip = avg + gamma * variance;
    
    // Also respect the hard min/max bounds
    minClip = max(minClip, mn);
    maxClip = min(maxClip, mx);
    
    vec3 histClamped = clipToAABB(histYCoCg, minClip, maxClip, avg);
    
    // Calculate blend factor based on how much history was clipped
    float clipDist = length(histYCoCg - histClamped);
    float histDist = length(histYCoCg - currYCoCg);
    float clipFactor = clamp(clipDist / max(histDist, 0.0001), 0.0, 1.0);
    
    // Reduce feedback when history is heavily clipped (motion/disocclusion)
    float feedback = mix(u_FeedbackMax, u_FeedbackMin, clipFactor);
    
    // Blend in YCoCg space
    vec3 resultYCoCg = mix(currYCoCg, histClamped, feedback);
    
    // Convert back to RGB
    vec3 outColor = YCoCg_to_RGB(resultYCoCg);
    
    // Clamp to valid range
    outColor = max(outColor, vec3(0.0));
    
    // Sharpening pass to counteract TAA blur
    // Using unsharp mask technique
    vec3 blur = vec3(0.0);
    blur += sampleCurrent(v_TexCoord + vec2(-texel.x, 0.0));
    blur += sampleCurrent(v_TexCoord + vec2(texel.x, 0.0));
    blur += sampleCurrent(v_TexCoord + vec2(0.0, -texel.y));
    blur += sampleCurrent(v_TexCoord + vec2(0.0, texel.y));
    blur *= 0.25;
    
    // Apply sharpening (reduced to prevent fireflies)
    float sharpenStrength = 0.15;
    outColor = outColor + (outColor - blur) * sharpenStrength;
    outColor = max(outColor, vec3(0.0));
    
    FragColor = vec4(outColor, 1.0);
}

