#version 330 core

layout (location = 0) in vec2 a_Position; // Unit quad [-1, 1]

uniform mat4 u_MVP;
uniform vec2 u_Center;
uniform float u_Radius;

out vec2 v_UV;

void main() {
    v_UV = a_Position;
    vec2 world_pos = u_Center + a_Position * (u_Radius * 1.25);
    gl_Position = u_MVP * vec4(world_pos, 0.0, 1.0);
}
