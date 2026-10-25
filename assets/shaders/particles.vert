#version 330 core

layout (location = 0) in vec2 a_Position;
layout (location = 1) in float a_Size;
layout (location = 2) in float a_Alpha;

uniform mat4 u_MVP;

out float v_Alpha;

void main() {
    v_Alpha = a_Alpha;
    gl_PointSize = a_Size;
    gl_Position = u_MVP * vec4(a_Position, 0.0, 1.0);
}
