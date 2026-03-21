#version 450 core

layout(location = 0) in vec2 v_texcoord;
layout(location = 0) out float g_occlusion;

layout(set = 0, binding = 0) uniform CameraData {
    mat4 view;
    mat4 projection;
    mat4 view_projection;
    mat4 inverse_view;
    mat4 inverse_projection;
    vec3 position;
    float padding;
} u_camera;

layout(set = 1, binding = 1) uniform sampler2D u_g_normal;
layout(set = 1, binding = 3) uniform sampler2D u_g_depth;

layout(set = 2, binding = 0) uniform SSAOKernel {
    vec4 samples[64];
} u_kernel;

layout(set = 2, binding = 1) uniform SSAOParams {
    vec2 noise_scale;
    float radius;
    float bias;
    int kernel_size;
    float power;
} u_params;

layout(set = 2, binding = 2) uniform sampler2D u_tex_noise;

vec3 decode_octahedron(vec2 f) {
    f = f * 2.0 - 1.0;
    vec3 n = vec3(f.x, f.y, 1.0 - abs(f.x) - abs(f.y));
    float t = clamp(-n.z, 0.0, 1.0);
    n.x += n.x >= 0.0 ? -t : t;
    n.y += n.y >= 0.0 ? -t : t;
    return normalize(n);
}

vec3 get_view_pos(vec2 uv) {
    float depth = texture(u_g_depth, uv).r;
    vec4 clip_space = vec4(uv * 2.0 - 1.0, depth, 1.0);
    clip_space.y = -clip_space.y;
    vec4 view_space = u_camera.inverse_projection * clip_space;
    return view_space.xyz / max(view_space.w, 0.000001);
}

void main() {
    float center_depth = texture(u_g_depth, v_texcoord).r;
    if (center_depth >= 1.0) {
        g_occlusion = 1.0;
        return;
    }

    vec3 normal = decode_octahedron(texture(u_g_normal, v_texcoord).rg);
    vec3 frag_pos = get_view_pos(v_texcoord);

    vec3 random_vec = texture(u_tex_noise, v_texcoord * u_params.noise_scale).xyz * 2.0 - 1.0;
    random_vec = normalize(random_vec);

    vec3 tangent = normalize(random_vec - normal * dot(random_vec, normal));
    if (length(tangent) < 0.0001) {
        tangent = abs(normal.z) < 0.999 ? normalize(cross(normal, vec3(0.0, 0.0, 1.0)))
                                        : normalize(cross(normal, vec3(0.0, 1.0, 0.0)));
    }
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN = mat3(tangent, bitangent, normal);

    int kernel_size = clamp(u_params.kernel_size, 1, 64);
    float occlusion = 0.0;

    for (int i = 0; i < kernel_size; ++i) {
        vec3 sample_pos = frag_pos + (TBN * u_kernel.samples[i].xyz) * u_params.radius;

        vec4 projected = u_camera.projection * vec4(sample_pos, 1.0);
        projected.xyz /= projected.w;
        vec2 sample_uv = projected.xy * 0.5 + 0.5;
        sample_uv.y = 1.0 - sample_uv.y;

        if (sample_uv.x < 0.0 || sample_uv.x > 1.0 || sample_uv.y < 0.0 || sample_uv.y > 1.0) {
            continue;
        }

        float sample_depth = get_view_pos(sample_uv).z;
        float depth_delta = max(abs(frag_pos.z - sample_depth), 0.0001);
        float range_check = smoothstep(0.0, 1.0, u_params.radius / depth_delta);
        float is_occluded = sample_depth >= (sample_pos.z + u_params.bias) ? 1.0 : 0.0;
        occlusion += is_occluded * range_check;
    }

    float ao = 1.0 - (occlusion / float(kernel_size));
    g_occlusion = pow(clamp(ao, 0.0, 1.0), u_params.power);
}
