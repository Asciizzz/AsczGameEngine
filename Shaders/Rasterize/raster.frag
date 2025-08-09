#version 450

layout(binding = 0) uniform GlobalUBO {
    mat4 proj;
    mat4 view;
} glb;

layout(binding = 1) uniform sampler2D txtrSmplr;

// Material uniform buffer
layout(binding = 2) uniform MaterialUBO {
    vec4 prop1; // <bool shading>, <int toonLevel>, <float normalBlend>, <float discardThreshold>
} material;

layout(location = 0) in vec2 fragTxtr;
layout(location = 1) in vec3 fragWorldNrml;
layout(location = 2) in vec3 fragWorldPos;
layout(location = 3) in vec4 fragInstanceColor;
layout(location = 4) in float vertexLightFactor;  // Pre-computed from vertex shader

layout(location = 0) out vec4 outColor;

void main() {
    vec4 texColor = texture(txtrSmplr, fragTxtr);
    float discardThreshold = material.prop1.w;

    if (texColor.a < discardThreshold) { discard; }

    // Normal material rendering
    float normalBlend = material.prop1.z;
    vec3 normal = normalize(fragWorldNrml);
    vec3 normalColor = (normal + 1.0) * 0.5;
    
    vec3 rgbColor = texColor.rgb + normalColor * normalBlend;
    vec3 rgbFinal = rgbColor * fragInstanceColor.rgb;
    float alpha = texColor.a * fragInstanceColor.a;

    // Use the pre-computed lighting from vertex shader
    outColor = vec4(rgbFinal * vertexLightFactor, alpha);
}