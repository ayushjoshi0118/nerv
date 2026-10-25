#version 330 core

in float v_ParamT;

uniform vec4 u_Color;
uniform float u_Alpha;
uniform float u_PulseProgress; // 0.0 to 1.0 along line
uniform float u_Time;

out vec4 FragColor;

void main() {
    // Base edge alpha
    float base_alpha = u_Alpha * 0.45;

    // Moving data pulse packet
    float pulse_width = 0.08;
    float dist_to_pulse = abs(v_ParamT - u_PulseProgress);
    float packet = 1.0 - smoothstep(0.0, pulse_width, dist_to_pulse);

    vec3 packet_color = vec3(0.365, 0.922, 1.0); // Electric Cyan
    vec3 col = mix(u_Color.rgb, packet_color, packet * 0.9);
    float alpha = clamp(base_alpha + packet * 0.85, 0.0, 1.0);

    FragColor = vec4(col, alpha);
}
