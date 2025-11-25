#version 450

layout(location = 0) in vec3 fragNrml;
layout(location = 1) in flat uvec4 fragOther;

struct Material {
    vec4 base;
    uvec4 tex1;
    uvec4 tex2;
};

layout(std430, set = 1, binding = 0) readonly buffer MatBuffer {
    Material materials[];
};

layout(location = 0) out vec4 outColor;

void main() {
    vec3 lightDir = vec3(1.0, 1.0, 1.0);

    Material mat = materials[fragOther.x];

    outColor = mat.base * abs(dot(normalize(fragNrml), normalize(lightDir)));
}
