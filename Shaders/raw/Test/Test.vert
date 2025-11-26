#version 450

layout(push_constant) uniform PushConstant {
    uvec4 props; // .x = material index, .y = vertex count
} pConst;

layout(location = 0) in vec4  inPos_Tu;
layout(location = 1) in vec4  inNrml_Tv;
layout(location = 2) in vec4  inTangent;
layout(location = 3) in uvec4 inBoneIDs;
layout(location = 4) in vec4  inBoneWs;

layout(location = 5) in vec4  model4_0;
layout(location = 6) in vec4  model4_1;
layout(location = 7) in vec4  model4_2;
layout(location = 8) in vec4  model4_3;
layout(location = 9) in uvec4 other; // .x = boneOffset, .y = boneCount, .z = mrphWsOffset, .w = mrphWsCount

layout(location = 0) out vec3 fragWorld;
layout(location = 1) out vec3 fragNrml;
layout(location = 2) out vec2 fragUV;
layout(location = 3) out vec3 fragTangent;
layout(location = 4) out vec4 fragOther;

layout(set = 0, binding = 0) uniform GlobalUBO {
    mat4 proj;
    mat4 view;
} glb;

layout (std430, set = 2, binding = 0) readonly buffer SkinBuffer {
    mat4 skinData[];
};

layout(std430, set = 3, binding = 0) readonly buffer MrphWeightsBuffer {
    float mrphWeights[];
};

struct Mrph {
    vec3 dPos;
    vec3 dNrml;
    vec3 dTang;
};
layout(std430, set = 4, binding = 0) readonly buffer MrphDeltaBuffer {
    Mrph mrphDeltas[];
};

void main() {
    mat4 model = mat4(model4_0, model4_1, model4_2, model4_3);

    vec3 basePos    = inPos_Tu.xyz;
    vec3 baseNormal = inNrml_Tv.xyz;
    vec3 baseTangent = inTangent.xyz;

// ----------------------------------

    uint mrphWsOffset = other.z;
    uint mrphWsCount = other.w;

    uint vertexCount = pConst.props.y;
    uint vertexId = gl_VertexIndex;

    if (mrphWsCount > 0 && vertexCount > 0) {
        for (uint m = 0; m < mrphWsCount; ++m) {
            float weight = mrphWeights[mrphWsOffset + m];

            if (abs(weight) < 0.0001) continue; // negligible

            uint morphIndex = m * vertexCount + vertexId;
            Mrph delta = mrphDeltas[morphIndex];

            basePos    += weight * delta.dPos;
            baseNormal += weight * delta.dNrml;
            baseTangent += weight * delta.dTang;
        }
    }

// ----------------------------------

    vec4 skinnedPos = vec4(0.0);
    vec3 skinnedNormal = vec3(0.0);
    vec3 skinnedTangent = vec3(0.0);

    uint boneOffset = other.x;
    uint boneCount = other.y;

    if (boneCount == 0) { // No skinning
        skinnedPos = vec4(basePos, 1.0);
        skinnedNormal = baseNormal;
        skinnedTangent = baseTangent;
    } else {
        for (uint i = 0; i < 4; ++i) {
            uint id = inBoneIDs[i] + boneOffset;

            float w = inBoneWs[i];
            mat4 boneMat = id < boneCount ? skinData[id] : mat4(1.0);

            skinnedPos     += w * (boneMat * vec4(basePos, 1.0));
            skinnedNormal  += w * mat3(boneMat) * baseNormal;
            skinnedTangent += w * mat3(boneMat) * baseTangent;
        }
    }

// ----------------------------------

    vec4 worldPos4 = model * skinnedPos;
    fragWorld = worldPos4.xyz;

    mat3 normalMat = transpose(inverse(mat3(model)));
    fragNrml = normalMat * skinnedNormal;
    fragUV = inPos_Tu.zw;
    fragTangent = skinnedTangent;

    gl_Position = glb.proj * glb.view * worldPos4;
}
