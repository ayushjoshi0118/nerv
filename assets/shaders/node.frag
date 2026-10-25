#version 330 core

in vec2 v_UV;

uniform vec4 u_Color;
uniform float u_Glow;
uniform float u_Alpha;
uniform float u_Time;
uniform int u_IsSelected;
uniform int u_IsHovered;

out vec4 FragColor;

void main() {
    float dist = length(v_UV);
    if (dist > 1.0) {
        discard;
    }

    // Node core boundary is at dist = 0.45
    float core_radius = 0.45;
    float core_mask = 1.0 - smoothstep(core_radius - 0.05, core_radius, dist);

    // Outer glow halo falloff from core to boundary (0.45 to 1.0)
    float glow_falloff = 0.0;
    if (dist > core_radius) {
        float norm_d = (dist - core_radius) / (1.0 - core_radius);
        glow_falloff = exp(-norm_d * 3.2) * u_Glow;
    }

    // Subtle breathing pulse inside the core
    float pulse = 0.95 + 0.08 * sin(u_Time * 4.0 + dist * 5.0);

    // Core color is brightened toward white in the center
    vec3 bright_core = mix(u_Color.rgb, vec3(1.0, 1.0, 1.0), 0.55 * (1.0 - dist / core_radius));
    vec3 final_rgb = bright_core * core_mask * pulse + u_Color.rgb * glow_falloff;

    // Selection ring
    if (u_IsSelected == 1) {
        float ring_radius = 0.65;
        float ring_thickness = 0.08;
        float ring = 1.0 - smoothstep(0.0, ring_thickness, abs(dist - ring_radius));
        float ring_pulse = 0.8 + 0.2 * sin(u_Time * 8.0);
        final_rgb += vec3(0.365, 0.922, 1.0) * ring * ring_pulse; // Electric cyan selection ring
    } else if (u_IsHovered == 1) {
        float hover_ring = 1.0 - smoothstep(0.0, 0.05, abs(dist - 0.55));
        final_rgb += vec3(1.0, 1.0, 1.0) * hover_ring * 0.5;
    }

    float final_alpha = (core_mask + glow_falloff * 0.7) * u_Alpha;
    FragColor = vec4(final_rgb, clamp(final_alpha, 0.0, 1.0));
}
