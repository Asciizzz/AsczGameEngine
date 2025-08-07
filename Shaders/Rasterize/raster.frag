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

layout(location = 0) out vec4 outColor;

// Toon shading function (branchless)
float applyToonShading(float value, int toonLevel) {
    // Convert toonLevel to float for calculations
    float level = float(toonLevel);
    
    // For level 0, return original value (no quantization)
    // For level > 0, apply quantization
    float bands = level + 1.0;
    float quantized = floor(value * bands + 0.5) / bands;
    
    // Use step function to select between original and quantized
    // step(1.0, level) returns 1.0 when level >= 1.0, 0.0 otherwise
    float useToon = step(1.0, level);
    float result = mix(value, quantized, useToon);
    
    return clamp(result, 0.0, 1.0);
}

void main() {
    vec4 texColor = texture(txtrSmplr, fragTxtr);
    float discardThreshold = material.prop1.w;

    if (texColor.a < discardThreshold) { discard; }

    // Unwrap the other generic material properties
    float shading = material.prop1.x;
    int toonLevel = int(material.prop1.y);
    float normalBlend = material.prop1.z;

    vec3 lightDir = normalize(vec3(-1.0, -1.0, -1.0));

    vec3 normal = normalize(fragWorldNrml);
    vec3 normalColor = (normal + 1.0) * 0.5;
    
    // The idea is that if shading is disabled, lightFactor should be 1.0
    float lightFactor = max(dot(normal, -lightDir), 1 - shading);
    lightFactor = length(fragWorldNrml) > 0.001 ? lightFactor : 1.0;

    // Apply toon shading based on toon level
    lightFactor = applyToonShading(lightFactor, toonLevel);
    lightFactor = 0.2 + lightFactor * 0.8; // Ambient

    // vec4 finalColor = texColor * fragInstanceColor;
    vec3 rgbColor = texColor.rgb + normalColor * normalBlend;

    vec3 rgbFinal = rgbColor * fragInstanceColor.rgb;

    float alpha = texColor.a * fragInstanceColor.a;

    outColor = vec4(rgbFinal * lightFactor, alpha);
}