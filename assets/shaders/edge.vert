#version 330 core

layout (location = 0) in vec2 a_Position;
layout (location = 1) in float a_ParamT;
layout (location = 2) in float a_ParamS;
layout (location = 3) in vec4 a_Color;
layout (location = 4) in float a_PulseProgress;
layout (location = 5) in float a_PulseSpeed;
layout (location = 6) in float a_Alpha;

uniform mat4 u_MVP;

out float v_ParamT;
out float v_ParamS;
out vec4 v_Color;
out float v_PulseProgress;
out float v_PulseSpeed;
out float v_Alpha;

void main() {
    v_ParamT = a_ParamT;
    v_ParamS = a_ParamS;
    v_Color = a_Color;
    v_PulseProgress = a_PulseProgress;
    v_PulseSpeed = a_PulseSpeed;
    v_Alpha = a_Alpha;

    gl_Position = u_MVP * vec4(a_Position, 0.0, 1.0);
}
