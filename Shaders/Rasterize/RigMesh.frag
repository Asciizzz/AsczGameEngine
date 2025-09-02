#version 450
#extension GL_EXT_nonuniform_qualifier : require      // for nonuniformEXT()
#extension GL_EXT_samplerless_texture_functions : enable // sometimes required by toolchains; optional

struct Material {
    vec4  shadingParams; // shadingFlag, toonLevel, normalBlend, discardThreshold
    uvec4 texIndices;    // albedo, albedoSampler, normal, normalSampler
};

layout(std430, set = 2, binding = 0) readonly buffer MaterialBuffer {
    Material materials[];
};

layout(set = 3, binding = 0) uniform texture2D textures[];
layout(set = 3, binding = 1) uniform sampler   samplers[];

layout(location = 0) in float debugLight;
layout(location = 1) in vec4 debugColor;
layout(location = 2) in vec2 fragUV;

layout(location = 0) out vec4 outColor;

vec4 getTexture(uint texIndex, uint addressMode, vec2 uv) {
    return texture(sampler2D(textures[nonuniformEXT(texIndex)], samplers[addressMode]), uv);
}

void main() {
    Material material = materials[4];

    uint albTexIndex = material.texIndices.x;
    uint albSamplerIndex = material.texIndices.y;
    vec4 texColor = getTexture(albTexIndex, albSamplerIndex, fragUV);

    texColor = debugColor;

    outColor = vec4(texColor.xyz, 1.0);
}
