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
    vec3 n = normalize(fragWorldNrml);

    // Remap from [-1, 1] to [0, 1]
    vec3 color = n * 0.5 + 0.5;

    // Optional: boost saturation or contrast for visibility
    color = pow(color, vec3(0.6));

    outColor = vec4(color, 1.0);
}
