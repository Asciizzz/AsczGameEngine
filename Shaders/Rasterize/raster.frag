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
layout(location = 2) in vec3 fragWorldPos;
layout(location = 3) in vec4 fragInstanceColor;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 lightPos = vec3(0.0, 100.0, 0.0);

    vec3 lightDir = normalize(lightPos - fragWorldPos);
    vec3 normal = normalize(fragWorldNrml);

    float lightFactor = max(dot(normal, lightDir), 0.0);
    // In case there's no normal
    lightFactor = length(fragWorldNrml) > 0.001 ? lightFactor : 1.0;

    // For toon shading effect
    // float toonFactor = ceil(lightFactor * 2.0) * 0.5;
    float toonFactor = lightFactor; // Ignore toon shading for now

    // Final brightness factor
    float finalFactor = 0.1 + toonFactor * 0.9;

    vec4 texColor = texture(txtrSmplr, fragTxtr);
    
    // Apply instance color multiplication (no material color since we use instances for coloring)
    vec4 finalColor = texColor * fragInstanceColor;
    
    outColor = vec4(finalColor.rgb * finalFactor, finalColor.a);
}