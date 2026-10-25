#version 330 core

in vec2 v_ScreenUV;

uniform vec2 u_Resolution;
uniform vec2 u_CameraPos;
uniform float u_CameraZoom;
uniform float u_Time;

out vec4 FragColor;

void main() {
    // 1. Deep cosmic radial vignette
    vec2 center_delta = v_ScreenUV - vec2(0.5);
    center_delta.x *= (u_Resolution.x / max(1.0, u_Resolution.y));
    float vignette_dist = length(center_delta);

    vec3 void_black = vec3(0.020, 0.027, 0.051); // #05070D
    vec3 deep_navy  = vec3(0.043, 0.071, 0.125); // #0B1220

    float vignette = smoothstep(0.0, 1.25, vignette_dist);
    vec3 bg_color = mix(deep_navy, void_black, vignette);

    // 2. Parallax Synaptic Reticulum (Subtle organic background grid)
    vec2 world_sample = (v_ScreenUV - vec2(0.5)) * (u_Resolution / max(1.0, u_CameraZoom)) + u_CameraPos;
    vec2 grid_uv = world_sample * 0.0035;

    // Subtle breathing hexagonal/orthogonal grid lines
    vec2 grid_dist = abs(fract(grid_uv - 0.5) - 0.5) / fwidth(grid_uv);
    float line_val = min(grid_dist.x, grid_dist.y);
    float grid_mask = 1.0 - min(line_val, 1.0);

    // Subtle pulsing filament color
    float pulse = 0.5 + 0.5 * sin(u_Time * 0.8 + grid_uv.x * 3.0 + grid_uv.y * 3.0);
    vec3 grid_color = vec3(0.365, 0.922, 1.0) * 0.035 * pulse;

    // Subtle central orbital ring in world space
    float world_radius = length(world_sample);
    float hub_glow = exp(-world_radius * 0.003) * 0.06;
    vec3 hub_glow_col = vec3(0.604, 0.482, 1.0) * hub_glow;

    vec3 final_color = bg_color + grid_color * grid_mask + hub_glow_col;
    FragColor = vec4(final_color, 1.0);
}
