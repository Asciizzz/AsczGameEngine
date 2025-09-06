#version 450
#extension GL_EXT_nonuniform_qualifier : require      // for nonuniformEXT()
#extension GL_EXT_samplerless_texture_functions : enable // sometimes required by toolchains; optional

layout(push_constant) uniform PushConstant {
    uvec4 properties;
} pushConstant;

struct Material {
    vec4  shadingParams; // shadingFlag, toonLevel, normalBlend, discardThreshold
    uvec4 texIndices;    // addressMode, albedo, normal, unused
};

layout(std430, set = 1, binding = 0) readonly buffer MaterialBuffer {
    Material materials[];
};

layout(set = 2, binding = 0) uniform texture2D textures[];
layout(set = 2, binding = 1) uniform sampler   samplers[];


layout(location = 0) in float debugLight;
layout(location = 1) in vec2 fragTexUV;

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
    Material material = materials[pushConstant.properties.x];

    uint addressMode = material.texIndices.x;

    // Albedo texture
    uint albTexIndex = material.texIndices.y;
    vec4 texColor = getTexture(albTexIndex, addressMode, fragTexUV);

    // Discard low opacity fragments
    float discardThreshold = material.shadingParams.w;
    if (texColor.a < discardThreshold) { discard; }

    // Apply toon shading
    uint toonLevel = uint(material.shadingParams.y);
    float finalLight = applyToonShading(debugLight, toonLevel);
    
    // Ambient
    // finalLight = 0.3 + finalLight * 0.7;
    finalLight = 0.7 + finalLight * 0.3;

    // Check if shading is disabled
    bool shadingFlag = material.shadingParams.x > 0.5;
    finalLight = shadingFlag ? finalLight : 1.0;

    vec3 finalColor = texColor.xyz * finalLight;

    outColor = vec4(finalColor, 1.0);
}
