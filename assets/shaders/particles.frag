#version 330 core

in float v_Alpha;

uniform vec4 u_Color;

out vec4 FragColor;

void main() {
    vec2 coord = gl_PointCoord - vec2(0.5);
    float dist = length(coord);
    if (dist > 0.5) {
        discard;
    }

    float soft_circle = 1.0 - smoothstep(0.2, 0.5, dist);
    FragColor = vec4(u_Color.rgb, u_Color.a * v_Alpha * soft_circle);
}
