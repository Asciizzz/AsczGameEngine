#version 450

layout(location = 1) in vec3 fragWorldNrml;
layout(location = 3) in vec4 fragInstanceColor;

layout(location = 0) out vec4 outColor;

void main() {
    if (fragInstanceColor.a < 0.001) discard;

    // Convert normal from [-1, 1] range to [0, 1] range for color visualization
    vec3 normal = normalize(fragWorldNrml);
    vec3 normalColor = (normal + 1.0) * 0.5;
    
    // Debug: Show normal as RGB color
    // Red = X component, Green = Y component, Blue = Z component
    vec4 debugColor = vec4(normalColor, 1.0);
    
    // No material color multiplication needed since instance coloring is handled elsewhere
    outColor = vec4(normalColor, fragInstanceColor.a);
}