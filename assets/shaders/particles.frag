#version 330 core

in float v_Alpha;
in float v_Depth;

uniform vec4 u_Color;

out vec4 FragColor;

void main() {
    vec2 coord = gl_PointCoord - vec2(0.5);
    float dist = length(coord);
    if (dist > 0.5) {
        discard;
    }

    // Bioluminescent bokeh disc with bright center nucleus
    float bokeh = 1.0 - smoothstep(0.12, 0.5, dist);
    float core = 1.0 - smoothstep(0.0, 0.16, dist);

    // Color gradient across depths: cyan (near) to neural violet (far)
    vec3 mote_col = mix(vec3(0.365, 0.922, 1.0), vec3(0.604, 0.482, 1.0), clamp(v_Depth, 0.0, 1.0));
    mote_col = mix(mote_col, vec3(1.0, 1.0, 1.0), core * 0.7);

    float alpha = v_Alpha * (bokeh * 0.7 + core * 0.5);
    FragColor = vec4(mote_col, clamp(alpha, 0.0, 1.0));
}
