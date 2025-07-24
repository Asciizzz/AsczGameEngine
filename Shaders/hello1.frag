#version 450

layout(binding = 1) uniform sampler2D txtrSmplr;

layout(location = 0) in vec2 fragTxtr;
layout(location = 1) in vec3 fragNrml;

layout(location = 0) out vec4 outColor;

void main() {
    // Convert normal from [-1, 1] range to [0, 1] range for color visualization
    vec3 normal = normalize(fragNrml);
    vec3 normalColor = (normal + 1.0) * 0.5;
    
    // Debug: Show normal as RGB color
    // Red = X component, Green = Y component, Blue = Z component
    outColor = vec4(normalColor, 1.0);
}