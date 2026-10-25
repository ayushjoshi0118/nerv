#version 330 core

in vec2 v_UV;

uniform vec4 u_Color;
uniform float u_Progress; // 0.0 at spawn to 1.0 at full dissipation

out vec4 FragColor;

void main() {
    float dist = length(v_UV);
    if (dist > 1.0 || dist < 0.2) {
        discard;
    }

    // Expanding wavefront ring
    float ring_pos = 0.8;
    float ring_width = 0.22 * (1.0 - u_Progress * 0.4);
    float ring = 1.0 - smoothstep(0.0, ring_width, abs(dist - ring_pos));

    // Fade out as progress approaches 1.0
    float alpha = ring * (1.0 - u_Progress) * 0.85;

    // Slight chromatic edge
    vec3 wave_col = mix(u_Color.rgb, vec3(1.0, 1.0, 1.0), 0.35 * ring);

    FragColor = vec4(wave_col, clamp(alpha, 0.0, 1.0));
}
