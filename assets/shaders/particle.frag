#version 410 core

out vec4 frag_color;

in vec2 v_texcoord;
in vec4 v_color;

uniform sampler2D u_particle_texture;
uniform bool u_use_texture;
uniform bool u_additive_blend; // For fire/glow effects

void main() {
    vec4 tex_color = vec4(1.0);
    
    if (u_use_texture) {
        tex_color = texture(u_particle_texture, v_texcoord);
    } else {
        // Circular falloff for untextured particles
        vec2 center = v_texcoord - 0.5;
        float dist = length(center) * 2.0;
        float falloff = 1.0 - smoothstep(0.0, 1.0, dist);
        tex_color = vec4(1.0, 1.0, 1.0, falloff);
    }
    
    // Multiply by instance color
    vec4 final_color = tex_color * v_color;
    
    // Discard fully transparent pixels
    if (final_color.a < 0.01) {
        discard;
    }
    
    frag_color = final_color;
}
