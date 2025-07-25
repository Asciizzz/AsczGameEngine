#version 450

layout(binding = 1) uniform sampler2D txtrSmplr;

layout(location = 0) in vec2 fragTxtr;
layout(location = 1) in vec3 fragNrml;
layout(location = 2) in vec3 fragWorldPos;  // World position from vertex shader

layout(location = 0) out vec4 outColor;

void main() {
    vec3 lightPos = vec3(0.0, 40.0, 0.0); // Example light position

    vec3 lightDir = normalize(lightPos - fragWorldPos);
    vec3 normal = normalize(fragNrml);

    float lightIntensity = abs(dot(normal, lightDir));
    lightIntensity = length(fragNrml) > 0.001 ? lightIntensity : 1.0;

    // For toon shading effect
    float diffFactor = ceil(lightIntensity * 4.0) * 0.25;
    diffFactor = 0.1 + diffFactor * 0.9;

    vec4 texColor = texture(txtrSmplr, fragTxtr);
    outColor = vec4(texColor.rgb * diffFactor, 1.0);

    // Mix original color with position-based effect (uncomment to see effect)
    // texColor.rgb = mix(texColor.rgb, positionColor, 0.3);
    
    // outColor = vec4(texColor.rgb, 1.0);
}