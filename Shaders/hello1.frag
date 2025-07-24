#version 450

layout(location = 0) in vec2 fragTxtr;
layout(location = 1) in vec3 fragNrml;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 lightDir = normalize(vec3(-1.0, -1.0, -1.0));
    vec3 normal = normalize(fragNrml);

    float lightIntensity = clamp(dot(normal, -lightDir), 0.1, 1.0);
    lightIntensity = normal == vec3(0.0) ? 1.0 : lightIntensity;

    // Apply UV map instead of texture
    float r = fract(fragTxtr.x * 10.0);
    float g = fract(fragTxtr.y * 10.0);
    float b = 0.5 + 0.5 * sin(fragTxtr.x * 10.0 + fragTxtr.y * 10.0);

    outColor = vec4(vec3(lightIntensity) * vec3(r, g, b), 1.0);
}