#version 450
#extension GL_EXT_nonuniform_qualifier : require      // for nonuniformEXT()
#extension GL_EXT_samplerless_texture_functions : enable // sometimes required by toolchains; optional

layout(location = 0) in float debugLight;
layout(location = 1) in vec4 debugColor;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(debugColor.xyz * debugLight, 1.0);
}
