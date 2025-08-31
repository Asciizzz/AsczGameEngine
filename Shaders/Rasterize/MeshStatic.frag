#version 450
#extension GL_EXT_nonuniform_qualifier : require      // for nonuniformEXT()
#extension GL_EXT_samplerless_texture_functions : enable // sometimes required by toolchains; optional

layout(set = 0, binding = 0) uniform GlobalUBO {
    mat4 proj;
    mat4 view;
    vec4 props; // General purpose: <float time>, <unused>, <unused>, <unused>
    vec4 cameraPos;    // xyz: camera position, w: fov
    vec4 cameraForward; // xyz: forward direction, w: aspect ratio
    vec4 cameraRight;   // xyz: camera right, w: near
    vec4 cameraUp;      // xyz: camera up, w: far
} glb;


struct Material {
    vec4 shadingParams; // <float shading>, <float toonLevel>, <float normalBlend>, <float unused>
    uvec4 texIndices;
};

layout(std430, set = 1, binding = 0) readonly buffer MaterialBuffer {
    Material materials[];
};

layout(set = 2, binding = 0) uniform sampler2D textures[];

layout(location = 0) in flat uvec4 fragProperties;
layout(location = 1) in vec4 fragMultColor;
layout(location = 2) in vec2 fragTxtr;
layout(location = 3) in vec3 fragWorldPos;
layout(location = 4) in vec3 fragWorldNrml;
layout(location = 5) in vec4 fragTangent;

layout(location = 0) out vec4 outColor;

float applyToonShading(float value, uint toonLevel) {
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
    Material material = materials[fragProperties.x];

    uint texIndex = material.texIndices.x;
    vec4 texColor = texture(textures[texIndex], fragTxtr);

    // Discard low opacity fragments
    float discardThreshold = material.shadingParams.w;
    if (texColor.a < discardThreshold) { discard; }

    // Normal mapping
    vec3 bitangent = cross(fragWorldNrml, fragTangent.xyz) * fragTangent.w;
    mat3 TBN = mat3(fragTangent.xyz, bitangent, fragWorldNrml);

    uint normalTexIndex = material.texIndices.y;
    vec3 mapN = texture(textures[normalTexIndex], fragTxtr).xyz * 2.0 - 1.0;

    // Use normal if no tangent is provided
    bool noTangent = fragTangent.w == 0.0;
    vec3 mappedNormal = noTangent ? fragWorldNrml : normalize(TBN * mapN);

    vec3 sunDir = normalize(vec3(1.0, 1.0, 1.0));

    // S1mple shading
    int shading = int(material.shadingParams.x); // Shading flag
    float lightFactor = max(dot(mappedNormal, sunDir), 1 - shading);

    // In case no normal
    lightFactor = length(mappedNormal) > 0.001 ? lightFactor : 1.0;

    // Toon shading
    uint toonLevel = uint(material.shadingParams.y);
    lightFactor = applyToonShading(lightFactor, toonLevel);
    lightFactor = 0.4 + lightFactor * 0.6; // Ambient

    // Combined values
    float finalAlpha = texColor.a * fragMultColor.a;
    vec3 finalRGB = texColor.rgb * fragMultColor.rgb * lightFactor;

    outColor = vec4(finalRGB, finalAlpha);
}
