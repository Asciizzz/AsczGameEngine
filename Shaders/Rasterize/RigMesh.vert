#version 450
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_samplerless_texture_functions : enable

layout(set = 0, binding = 0) uniform GlobalUBO {
    mat4 proj;
    mat4 view;
} glb;

layout(std430, set = 1, binding = 0) buffer BoneBuffer {
    mat4 inverseBindMatrices[];
};

layout(location = 0) in vec4 inPos_Tu;
layout(location = 1) in vec4 inNrml_Tv;
layout(location = 2) in vec4 inTangent;
layout(location = 3) in uvec4 inBoneID;
layout(location = 4) in vec4 inWeights;

// No instance data yet, i am scratching my head here
layout(location = 0) out float debugLight;
layout(location = 1) out vec4 debugColor;
layout(location = 2) out vec2 fragUV;

void main() {
    // Get the average inverse bind matrix for the bones
    mat4 avgInverseBindMatrix = mat4(0.0);
    for (int i = 0; i < 4; ++i) {
        avgInverseBindMatrix += inverseBindMatrices[inBoneID[i]];
    }
    avgInverseBindMatrix /= 4.0;

    vec4 worldPos = avgInverseBindMatrix * vec4(inPos_Tu.xyz, 1.0);

    vec3 normal = inNrml_Tv.xyz;

    debugLight = dot(normal, normalize(vec3(0.0, 1.0, 1.0)));

    debugColor = inWeights;

    fragUV = vec2(inPos_Tu.w, inNrml_Tv.w);

    gl_Position = glb.proj * glb.view * worldPos;
}