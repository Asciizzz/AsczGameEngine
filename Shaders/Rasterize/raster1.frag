#version 450

// Global uniform buffer for camera matrices
layout(binding = 0) uniform GlobalUBO {
    mat4 proj;
    mat4 view;
} glb;

layout(binding = 1) uniform sampler2D txtrSmplr;

// Material uniform buffer (reserved for future material properties)
layout(binding = 2) uniform MaterialUBO {
    float padding;
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
    
    // No material color multiplication needed since instance coloring is handled elsewhere
    outColor = debugColor;
}