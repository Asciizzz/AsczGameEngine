#version 450

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
    vec4 texColor = texture(txtrSmplr, fragTxtr);
    if (texColor.a < 0.001) { discard; }

    vec3 lightPos = vec3(0.0, 1000.0, 0.0);

    vec3 lightDir = normalize(lightPos - fragWorldPos);
    vec3 normal = normalize(fragWorldNrml);

    float lightFactor = max(dot(normal, lightDir), 0.0);
    lightFactor = length(fragWorldNrml) > 0.001 ? lightFactor : 1.0;

    // float finalFactor = 0.01 + lightFactor * 0.99;
    float finalFactor = 1.0; // Turn off lighting for now

    vec4 finalColor = texColor * fragInstanceColor;

    outColor = vec4(finalColor.rgb * finalFactor, finalColor.a);
}