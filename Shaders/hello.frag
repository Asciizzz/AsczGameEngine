#version 450

layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) in vec2 fragTxtr;
layout(location = 1) in vec3 fragNrml;

layout(location = 0) out vec4 outColor;

void main() {
    // Sample the texture
    vec4 texColor = texture(texSampler, fragTxtr);
    
    // Simple directional lighting
    vec3 lightDir = normalize(vec3(0.0, -1.0, 0.0));
    vec3 normal = normalize(fragNrml);
    
    // Calculate lighting intensity using dot product
    // Clamp between 0.3 and 1.0 for nice ambient + directional lighting
    float lightIntensity = clamp(dot(normal, -lightDir), 0.1, 1.0);

    
    // Apply lighting to texture color
    outColor = vec4(texColor.rgb * lightIntensity, texColor.a);
}