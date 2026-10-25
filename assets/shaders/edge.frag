#version 330 core

in float v_ParamT;
in float v_ParamS;
in vec4 v_Color;
in float v_PulseProgress;
in float v_PulseSpeed;
in float v_Alpha;

uniform float u_Time;

out vec4 FragColor;

void main() {
    // 1. Exponential cross-filament antialiased core falloff
    float cross_falloff = exp(-abs(v_ParamS) * 3.5);

    // 2. Continuous flowing multi-signal data stream
    float phase1 = fract(v_ParamT * 2.2 - u_Time * (v_PulseSpeed * 0.8) + v_PulseProgress);
    float pulse1 = 1.0 - smoothstep(0.0, 0.08, abs(phase1 - 0.5));

    float phase2 = fract(v_ParamT * 1.4 - u_Time * (v_PulseSpeed * 0.5) + v_PulseProgress + 0.33);
    float pulse2 = 1.0 - smoothstep(0.0, 0.06, abs(phase2 - 0.5));

    float signals = max(pulse1, pulse2);

    // Base filament color
    vec3 base_col = v_Color.rgb * cross_falloff * 0.75;

    // Glowing signal packet color (Electric Cyan)
    vec3 signal_col = vec3(0.365, 0.922, 1.0) * signals * cross_falloff * 2.0;

    // Combined color with HDR glow
    vec3 final_rgb = base_col + signal_col;
    float final_alpha = (cross_falloff * 0.4 + signals * 0.8) * v_Alpha;

    FragColor = vec4(final_rgb, clamp(final_alpha, 0.0, 1.0));
}
