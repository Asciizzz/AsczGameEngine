#version 450
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_samplerless_texture_functions : enable

layout(set = 0, binding = 0) uniform GlobalUBO {
    mat4 proj;
    mat4 view;
} glb;

layout(std430, set = 3, binding = 0) readonly buffer RigBuffer {
    mat4 finalPose[];
};

layout(location = 0) in vec4 inPos_Tu;
layout(location = 1) in vec4 inNrml_Tv;
layout(location = 2) in vec4 inTangent;
layout(location = 3) in uvec4 inBoneID;
layout(location = 4) in vec4 inWeights;

// No instance data yet, i am scratching my head here
layout(location = 0) out float debugLight;
layout(location = 1) out vec2 fragUV;


void main() {
    // --- Apply morph targets ---
    vec3 basePos   = inPos_Tu.xyz;
    vec3 baseNormal = inNrml_Tv.xyz;

    // basePos   += morphWeights[0] * morphPosDelta0;
    // baseNormal += morphWeights[0] * morphNrmlDelta0;
    // basePos   += morphWeights[1] * morphPosDelta1;
    // baseNormal += morphWeights[1] * morphNrmlDelta1;

    // --- Skinning ---
    vec4 skinnedPos = vec4(0.0);
    vec3 skinnedNormal = vec3(0.0);

    for (uint i = 0; i < 4; ++i) {
        uint id = inBoneID[i];
        float w = inWeights[i];
        mat4 boneMat = finalPose[id];

        skinnedPos    += w * (boneMat * vec4(basePos, 1.0));
        skinnedNormal += w * mat3(boneMat) * baseNormal;
    }

    vec4 worldPos = skinnedPos;
    vec3 normal   = normalize(skinnedNormal);

    gl_Position = glb.proj * glb.view * worldPos;

    debugLight = dot(normal, normalize(vec3(1.0, 1.0, 1.0)));
    fragUV = vec2(inPos_Tu.w, inNrml_Tv.w);
}