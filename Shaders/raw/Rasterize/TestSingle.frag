#version 450

layout(location = 0) in vec2 fragTexUV;
layout(location = 1) in vec3 fragWorldPos;
layout(location = 2) in vec3 fragWorldNrml;
layout(location = 3) in vec4 fragTangent;
layout(location = 4) in flat uint fragMaterialIndex;
layout(location = 5) in flat uint fragSpecial;

layout(location = 0) out vec4 outColor;

// Create an array of rainbow colors
vec4 rainbowColors[7] = vec4[7](
    vec4(1.0, 0.5, 0.5, 1.0), // Red
    vec4(1.0, 0.8, 0.5, 1.0), // Orange
    vec4(1.0, 1.0, 0.5, 1.0), // Yellow
    vec4(0.5, 1.0, 0.5, 1.0), // Green
    vec4(0.5, 0.5, 1.0, 1.0), // Blue
    vec4(0.6, 0.2, 1.0, 1.0), // Indigo
    vec4(0.8, 0.5, 1.0, 1.0)  // Violet
);

void main() {
    float intensity = abs(dot(fragWorldNrml, normalize(vec3(1.0, 1.0, 1.0))));
    intensity = 0.1 + intensity * 0.9;

    // // For the time being, set the color based on the material index % 7
    // outColor = rainbowColors[fragMaterialIndex % 7] * intensity;

    // Highlight special fragments in red
    vec3 color = (fragSpecial == 1u) ? vec3(1.0, 0.6, 0.6) : vec3(1.0, 1.0, 1.0);
    outColor = vec4(color * intensity, 1.0);
}
