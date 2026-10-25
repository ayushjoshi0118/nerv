#version 330 core

layout (location = 0) in vec2 a_Position; // Line strip or quad vertex
layout (location = 1) in float a_ParamT;  // 0.0 at start, 1.0 at end

uniform mat4 u_MVP;

out float v_ParamT;

void main() {
    v_ParamT = a_ParamT;
    gl_Position = u_MVP * vec4(a_Position, 0.0, 1.0);
}
