#version 450

layout(location = 0) in vec2 fragTexUV;
layout(location = 1) in vec3 fragWorldPos;
layout(location = 2) in vec3 fragWorldNrml;
layout(location = 3) in vec4 fragTangent;
layout(location = 4) in flat uint fragMaterialIndex;

layout(location = 0) out vec4 outColor;

// Create an array of rainbow colors
vec4 rainbowColors[7] = vec4[7](
    vec4(1.0, 0.0, 0.0, 1.0), // Red
    vec4(1.0, 0.5, 0.0, 1.0), // Orange
    vec4(1.0, 1.0, 0.0, 1.0), // Yellow
    vec4(0.0, 1.0, 0.0, 1.0), // Green
    vec4(0.0, 0.0, 1.0, 1.0), // Blue
    vec4(0.29, 0.0, 0.51, 1.0), // Indigo
    vec4(0.56, 0.0, 1.0, 1.0)  // Violet
);

void main() {
    float intensity = abs(dot(normalize(fragWorldNrml), vec3(0.0, 1.0, 0.0)));
    intensity = 0.5 + intensity * 0.5;

    // For the time being, set the color based on the material index % 7
    outColor = rainbowColors[fragMaterialIndex % 7] * intensity;

    // outColor = vec4(vec3(intensity), 1.0);
}
