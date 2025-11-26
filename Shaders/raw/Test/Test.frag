#version 450

layout(push_constant) uniform PushConstant {
    uvec4 props;
} pConst;

layout(location = 0) in vec3 fragWorld;
layout(location = 1) in vec3 fragNrml;
layout(location = 2) in vec2 fragUV;
layout(location = 3) in vec3 fragTangent;
layout(location = 4) in vec4 fragOther;

struct Material {
    vec4 base;
    vec4 empty0;
    vec4 empty1;
    vec4 empty2;
    uvec4 tex1;
    uvec4 tex2;
};

layout(std430, set = 1, binding = 0) readonly buffer MatBuffer {
    Material materials[];
};

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

    Material mat = materials[pConst.props.x];

    outColor = mat.base * abs(dot(normalize(fragNrml), normalize(lightDir)));
}

