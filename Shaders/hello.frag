#version 450

layout(binding = 1) uniform sampler2D txtrSmplr;

layout(location = 0) in vec2 fragTxtr;
layout(location = 1) in vec3 fragNrml;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 lightDir = normalize(vec3(-1.0, -1.0, 0.0));
    vec3 normal = normalize(fragNrml);

    float nlength = length(fragNrml); // Check original normal length before normalization

    float lightIntensity = clamp(dot(normal, -lightDir), 0.1, 1.0);
    // If normal length is too small (invalid normal), use ambient lighting
    lightIntensity = nlength > 0.001 ? lightIntensity : 0.5;

    // Apply lighting to texture color
    vec4 texColor = texture(txtrSmplr, fragTxtr);
    outColor = vec4(texColor.rgb * lightIntensity, texColor.a);
}