#version 330 core

in vec2 v_ScreenUV;

uniform sampler2D u_SceneTexture;
uniform sampler2D u_BloomTexture;
uniform float u_BloomIntensity;
uniform int u_BloomEnabled;

out vec4 FragColor;

void main() {
    vec3 scene_col = texture(u_SceneTexture, v_ScreenUV).rgb;

    if (u_BloomEnabled == 1) {
        vec3 bloom_col = texture(u_BloomTexture, v_ScreenUV).rgb;
        scene_col += bloom_col * u_BloomIntensity;
    }

    // Restrained tone mapping to prevent clipping while keeping contrast
    vec3 mapped = scene_col / (scene_col + vec3(0.18)) * 1.18;

    FragColor = vec4(mapped, 1.0);
}
