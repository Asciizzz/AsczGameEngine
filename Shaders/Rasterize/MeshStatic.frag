#version 450
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_samplerless_texture_functions : enable

layout(set = 0, binding = 0) uniform GlobalUBO {
    mat4 proj;
    mat4 view;
    vec4 props;          // <time, unused, unused, unused>
    vec4 cameraPos;      // xyz: camera position, w: fov
    vec4 cameraForward;  // xyz: forward, w: aspect
    vec4 cameraRight;    // xyz: right, w: near
    vec4 cameraUp;       // xyz: up, w: far
} glb;

struct Material {
    vec4  shadingParams; // <shadingFlag, toonLevel, normalBlend, discardThreshold>
    uvec4 texIndices;    // x: albedo, y: normal, z/w: reserved
};

layout(std430, set = 1, binding = 0) readonly buffer MaterialBuffer {
    Material materials[];
};

layout(set = 2, binding = 0) uniform texture2D textures[];
layout(set = 2, binding = 1) uniform sampler   samplers[];

layout(location = 0) in flat uvec4 fragProperties;
layout(location = 1) in vec4 fragMultColor;
layout(location = 2) in vec2 fragUV;
layout(location = 3) in vec3 fragWorldPos;
layout(location = 4) in vec3 fragWorldNrml;
layout(location = 5) in vec4 fragTangent;

layout(location = 0) out vec4 outColor;

float applyToonShading(float value, uint toonLevel) {
    float level = float(toonLevel);
    float bands = level + 1.0;
    float quantized = floor(value * bands + 0.5) / bands;
    float useToon = step(1.0, level);
    float result = mix(value, quantized, useToon);
    return clamp(result, 0.0, 1.0);
}

vec4 getTexture(uint texIndex, uint addressMode, vec2 uv) {
    return texture(sampler2D(textures[nonuniformEXT(texIndex)], samplers[addressMode]), uv);
}


void main() {
    Material material = materials[fragProperties.x];

    // Albedo texture
    uint albTexIndex = material.texIndices.x;
    uint albTexMode = material.texIndices.y;
    vec4 texColor = getTexture(albTexIndex, albTexMode, fragUV);

    // Discard low opacity fragments
    float discardThreshold = material.shadingParams.w;
    if (texColor.a < discardThreshold) { discard; }

    // Normal mapping
    uint nrmlTexIndex = material.texIndices.z;
    uint nrmlTexMode = material.texIndices.w;
    vec3 mapNrml = getTexture(nrmlTexIndex, nrmlTexMode, fragUV).xyz * 2.0 - 1.0;

    vec3 bitangent = cross(fragWorldNrml, fragTangent.xyz) * fragTangent.w;
    mat3 TBN = mat3(fragTangent.xyz, bitangent, fragWorldNrml);

    // Ignore if no tangent
    bool noTangent = fragTangent.w == 0.0;
    vec3 mappedNormal = noTangent ? fragWorldNrml : normalize(TBN * mapNrml);

    // Simple directional light
    vec3 sunDir = normalize(vec3(1.0, 1.0, 1.0));
    int shading = int(material.shadingParams.x);
    float lightFactor = max(dot(mappedNormal, sunDir), 1 - shading);

    // Ignore if no normal
    lightFactor = length(mappedNormal) > 0.001 ? lightFactor : 1.0;

    uint toonLevel = uint(material.shadingParams.y);
    lightFactor = applyToonShading(lightFactor, toonLevel);
    lightFactor = 0.4 + lightFactor * 0.6;

    // Final color
    float finalAlpha = texColor.a * fragMultColor.a;
    vec3 finalRGB   = texColor.rgb * fragMultColor.rgb * lightFactor;

    outColor = vec4(finalRGB, finalAlpha);
}
