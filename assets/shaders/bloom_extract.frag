#version 330 core

in vec2 v_ScreenUV;

uniform sampler2D u_SceneTexture;
uniform float u_Threshold;

out vec4 FragColor;

void main() {
    vec4 color = texture(u_SceneTexture, v_ScreenUV);
    // Standard relative luminance weights
    float brightness = dot(color.rgb, vec3(0.2126, 0.7152, 0.0722));
    if (brightness > u_Threshold) {
        float factor = (brightness - u_Threshold) / max(brightness, 0.001);
        FragColor = vec4(color.rgb * factor, 1.0);
    } else {
        FragColor = vec4(0.0, 0.0, 0.0, 1.0);
    }
}
