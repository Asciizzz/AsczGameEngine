#version 450

// Global uniform buffer for camera matrices
layout(binding = 0) uniform GlobalUBO {
    mat4 proj;
    mat4 view;
} glb;

layout(binding = 1) uniform sampler2D txtrSmplr;

// Material uniform buffer
layout(binding = 2) uniform MaterialUBO {
    vec4 multColor;
} material;

layout(location = 0) in vec2 fragTxtr;
layout(location = 1) in vec3 fragWorldNrml;

layout(location = 0) out vec4 outColor;

void main() {
    // Convert normal from [-1, 1] range to [0, 1] range for color visualization
    vec3 normal = normalize(fragWorldNrml);
    vec3 normalColor = (normal + 1.0) * 0.5;
    
    // Debug: Show normal as RGB color
    // Red = X component, Green = Y component, Blue = Z component
    vec4 debugColor = vec4(normalColor, 1.0);
    
    // Apply material color multiplication to debug output too
    outColor = debugColor * material.multColor;
}