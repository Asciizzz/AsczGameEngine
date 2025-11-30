#version 450
#extension GL_EXT_nonuniform_qualifier : require      // for nonuniformEXT()
#extension GL_EXT_samplerless_texture_functions : enable // sometimes required by toolchains; optional

layout(push_constant) uniform PushConstant {
    uvec4 data0;
    uvec4 data1;
    uvec4 data2;
} pConst;

layout(location = 0) in vec3 fragWorld;
layout(location = 1) in vec3 fragNrml;
layout(location = 2) in vec2 fragUV;
layout(location = 3) in vec3 fragTangent;
layout(location = 4) in vec4 fragColor;

struct Material {
    vec4 base;
    vec4 empty0;
    vec4 empty1;
    vec4 empty2;
    uvec4 tex1;
    uvec4 tex2;
};

layout(std430, set = 2, binding = 0) readonly buffer MatBuffer {
    Material materials[];
};

layout(set = 3, binding = 0) uniform sampler2D textures[];
vec4 getTexture(uint texIndex, vec2 uv) {
    return texture(textures[nonuniformEXT(texIndex)], uv);
}

layout(location = 0) out vec4 outColor;

// Array of rainbow colors for testing
vec4 rainbowColors[7] = vec4[](
    vec4(1.0, 0.0, 0.0, 1.0),
    vec4(1.0, 0.5, 0.0, 1.0),
    vec4(1.0, 1.0, 0.0, 1.0),
    vec4(0.0, 1.0, 0.0, 1.0),
    vec4(0.0, 0.0, 1.0, 1.0),
    vec4(0.29, 0.0, 0.51, 1.0),
    vec4(0.56, 0.0, 1.0, 1.0)
);

void main() {
    vec3 lightDir = vec3(1.0, 1.0, 1.0);
    float lDot = dot(normalize(fragNrml), normalize(lightDir));
    lDot = clamp(lDot, 0.0, 1.0);

    lDot = mix(0.8, 1.0, lDot);

    Material mat = materials[pConst.data0.w];

    vec4 albColor = getTexture(mat.tex1.x, fragUV);
    vec4 baseColor = mat.base;
    
    float alpha = baseColor.a * albColor.a;
    if (alpha < 0.7) discard;

    vec3 fColor = baseColor.rgb * albColor.rgb;

    outColor = vec4(fColor * lDot, alpha);
}

