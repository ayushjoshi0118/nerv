#version 330 core

// Per-vertex quad corner attribute
layout (location = 0) in vec2 a_QuadCorner; // [-1, 1]

// Per-instance attributes (Instanced rendering!)
layout (location = 1) in vec2 a_InstancePos;
layout (location = 2) in float a_InstanceRadius;
layout (location = 3) in float a_InstanceGlow;
layout (location = 4) in vec4 a_InstanceColor;
layout (location = 5) in float a_InstanceAlpha;
layout (location = 6) in float a_InstanceActivity;
layout (location = 7) in vec2 a_InstanceFlags; // x: is_selected, y: is_hovered
layout (location = 8) in float a_InstanceCpu;

uniform mat4 u_MVP;

out vec2 v_UV;
out vec4 v_Color;
out float v_Glow;
out float v_Alpha;
out float v_Activity;
out vec2 v_Flags;
out float v_Cpu;

void main() {
    v_UV = a_QuadCorner;
    v_Color = a_InstanceColor;
    v_Glow = a_InstanceGlow;
    v_Alpha = a_InstanceAlpha;
    v_Activity = a_InstanceActivity;
    v_Flags = a_InstanceFlags;
    v_Cpu = a_InstanceCpu;

    // Generous quad expansion for atmospheric glow corona & targeting reticle
    vec2 world_pos = a_InstancePos + a_QuadCorner * (a_InstanceRadius * 2.8);
    gl_Position = u_MVP * vec4(world_pos, 0.0, 1.0);
}
