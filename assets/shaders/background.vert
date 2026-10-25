#version 330 core

layout (location = 0) in vec2 a_Position; // Fullscreen quad [-1, 1]

out vec2 v_ScreenUV;

void main() {
    v_ScreenUV = a_Position * 0.5 + 0.5; // [0, 1]
    gl_Position = vec4(a_Position, 0.0, 1.0);
}
