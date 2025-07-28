#version 450

// Global uniform buffer for camera matrices
layout(binding = 0) uniform GlobalUBO {
    mat4 projView;
} glb;

layout(binding = 1) uniform sampler2D txtrSmplr;

layout(location = 0) in vec2 fragTxtr;
layout(location = 1) in vec3 fragWorldNrml;
layout(location = 2) in vec3 fragWorldPos;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 lightPos = vec3(0.0, 0.5, 0.5); // Example light position

    vec3 lightDir = normalize(lightPos - fragWorldPos);
    vec3 normal = normalize(fragWorldNrml);

    float lightIntensity = max(dot(normal, lightDir), 0.0);
    // In case there's no normal
    lightIntensity = length(fragWorldNrml) > 0.001 ? lightIntensity : 1.0;

    // For toon shading effect
    float diffFactor = ceil(lightIntensity * 2.0) * 0.5;
    float finalFactor = 0.03 + diffFactor * 0.97;

    vec4 texColor = texture(txtrSmplr, fragTxtr);
    outColor = vec4(texColor.rgb * finalFactor, 1.0);
}