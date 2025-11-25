#version 450

layout(location = 0) in vec3 fragNrml;
layout(location = 1) in flat uvec4 fragOther;

layout(location = 0) out vec4 outColor;

// list of rainbow colors

vec3 rainbowColors[7] = vec3[](
    vec3(1.0, 0.0, 0.0), // Red
    vec3(1.0, 0.5, 0.0), // Orange
    vec3(1.0, 1.0, 0.0), // Yellow
    vec3(0.0, 1.0, 0.0), // Green
    vec3(0.0, 0.0, 1.0), // Blue
    vec3(0.29, 0.0, 0.51), // Indigo
    vec3(0.56, 0.0, 1.0)  // Violet
);

void main() {
    vec3 lightDir = vec3(1.0, 1.0, 1.0);

    vec4 color = vec4(rainbowColors[fragOther.x % 7], 1.0);

    outColor = color * abs(dot(normalize(fragNrml), normalize(lightDir)));
}
