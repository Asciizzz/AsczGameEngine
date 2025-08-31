#version 450
#extension GL_EXT_nonuniform_qualifier : require      // for nonuniformEXT()
#extension GL_EXT_samplerless_texture_functions : enable // sometimes required by toolchains; optional

struct Material {
    vec4 shadingParams;
    uvec4 texIndices;
};

layout(std430, set = 1, binding = 0) readonly buffer MaterialBuffer {
    Material materials[];
};

layout(set = 2, binding = 0) uniform sampler2D textures[];

layout(location = 0) in float debugLight;
layout(location = 1) in vec4 debugColor;
layout(location = 2) in vec2 fragUV;

layout(location = 0) out vec4 outColor;

void main() {
    Material material = materials[4]; // Get the first material (for simplicity)

    vec4 albedo = texture(textures[material.texIndices.x], fragUV);

    outColor = vec4((albedo.xyz + debugColor.xyz) * debugLight, 1.0);
}
