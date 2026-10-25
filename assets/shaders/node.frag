#version 330 core

in vec2 v_UV;
in vec4 v_Color;
in float v_Glow;
in float v_Alpha;
in float v_Activity;
in vec2 v_Flags; // x = is_selected, y = is_hovered
in float v_Cpu;

uniform float u_Time;

out vec4 FragColor;

void main() {
    float dist = length(v_UV);
    if (dist > 1.0) {
        discard;
    }

    float core_radius = 0.38;
    float core_mask = 1.0 - smoothstep(core_radius - 0.04, core_radius, dist);

    // 1. Atmospheric Outer Corona (Exponential falloff)
    float corona = 0.0;
    if (dist > core_radius * 0.8) {
        float norm_d = (dist - core_radius * 0.8) / (1.0 - core_radius * 0.8);
        corona = exp(-norm_d * 3.4) * v_Glow;
    }

    // 2. Membrane Fresnel Rim Light
    float rim = smoothstep(core_radius - 0.08, core_radius, dist) *
                (1.0 - smoothstep(core_radius, core_radius + 0.06, dist));

    // 3. Hot Bioluminescent Nucleus & CPU Pulse
    float pulse = 0.94 + 0.08 * sin(u_Time * (4.0 + v_Activity * 8.0) + dist * 5.0);
    float nucleus = 1.0 - smoothstep(0.0, 0.16, dist);

    // Color gradient
    vec3 bright_core = mix(v_Color.rgb, vec3(1.0, 1.0, 1.0), nucleus * 0.85);
    vec3 col = bright_core * core_mask * pulse;

    // Add rim light highlight
    vec3 rim_col = mix(v_Color.rgb, vec3(0.365, 0.922, 1.0), 0.6);
    col += rim_col * rim * 1.6;

    // Add outer corona
    col += v_Color.rgb * corona * 0.8;

    // 4. High CPU Plasma Corona Rays (> 40% CPU)
    if (v_Cpu > 40.0) {
        float angle = atan(v_UV.y, v_UV.x);
        float ray_pattern = 0.5 + 0.5 * sin(angle * 8.0 + u_Time * 6.0);
        float ray_corona = exp(-dist * 4.0) * ray_pattern * ((v_Cpu - 40.0) / 60.0);
        col += vec3(1.0, 0.4, 0.5) * ray_corona * 1.2;
    }

    // 5. Holographic Targeting Reticle (Selected)
    if (v_Flags.x > 0.5) {
        // Dual concentric targeting rings
        float inner_ring = 1.0 - smoothstep(0.0, 0.03, abs(dist - 0.54));
        float outer_ring = 1.0 - smoothstep(0.0, 0.025, abs(dist - 0.72));

        // Rotating crosshair notches
        float angle = atan(v_UV.y, v_UV.x) + u_Time * 1.5;
        float notch = pow(abs(cos(angle * 2.0)), 16.0);
        float ring_combo = inner_ring + outer_ring * notch;

        vec3 reticle_col = vec3(0.365, 0.922, 1.0); // Electric Cyan
        col += reticle_col * ring_combo * 1.4;
    } else if (v_Flags.y > 0.5) {
        // Hovered subtle focus ring
        float hover_ring = 1.0 - smoothstep(0.0, 0.035, abs(dist - 0.48));
        col += vec3(0.604, 0.482, 1.0) * hover_ring * 0.9;
    }

    float final_alpha = (core_mask + corona * 0.7 + rim * 0.8) * v_Alpha;
    FragColor = vec4(col, clamp(final_alpha, 0.0, 1.0));
}
