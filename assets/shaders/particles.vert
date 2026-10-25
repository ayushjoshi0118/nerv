#version 330 core

layout (location = 0) in vec2 a_Position;
layout (location = 1) in float a_Size;
layout (location = 2) in float a_Alpha;
layout (location = 3) in float a_Depth;

uniform mat4 u_MVP;

out float v_Alpha;
out float v_Depth;

void main() {
    v_Alpha = a_Alpha;
    v_Depth = a_Depth;
    gl_PointSize = a_Size;
    gl_Position = u_MVP * vec4(a_Position, 0.0, 1.0);
}
