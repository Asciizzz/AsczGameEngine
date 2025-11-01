#version 450

layout(push_constant) uniform PushConstant {
    mat4 model;
    uvec4 props1; // .x = bone count, .y = morph target count, .z = vertex count (for morphs)
} pConst;

layout(set = 0, binding = 0) uniform GlobalUBO {
    mat4 proj;
    mat4 view;
} glb;

// set 1 is material 

layout(std430, set = 2, binding = 0) readonly buffer SkinBuffer {
    mat4 skinData[];
};


struct Mrph {
    vec3 dPos;
    vec3 dNrml;
    vec3 dTan;
};
layout(std430, set = 3, binding = 0) readonly buffer MrphDeltaBuffer {
    Mrph morphs[];
};

layout(std430, set = 4, binding = 0) readonly buffer MrphWeightsBuffer {
    float morphWeights[];
};

layout(location = 0) in vec4  inPos_Tu;   // .xyz = pos, .w = u (if you use packed UVs)
layout(location = 1) in vec4  inNrml_Tv;  // .xyz = normal, .w = v (if packed)
layout(location = 2) in vec4  inTangent;  // .xyz = tangent, .w = handedness
layout(location = 3) in uvec4 inBoneIDs;
layout(location = 4) in vec4  inBoneWs;

layout(location = 0) out vec2 fragTexUV;
layout(location = 1) out vec3 fragWorldPos;
layout(location = 2) out vec3 fragWorldNrml;
layout(location = 3) out vec4 fragTangent;

void main() {

    // --- Apply morph targets ---
    vec3 basePos    = inPos_Tu.xyz;
    vec3 baseNormal = inNrml_Tv.xyz;
    vec3 baseTangent = inTangent.xyz;

    uint morphCount = pConst.props1.y;
    uint vertexCount = pConst.props1.z;
    uint vertexId = gl_VertexIndex;

    // Apply all morph targets
    if (morphCount > 0 && vertexCount > 0) {
        for (uint m = 0; m < morphCount; ++m) {
            float weight = morphWeights[m];

            if (abs(weight) < 0.0001) continue; // negligible

            uint morphIndex = m * vertexCount + vertexId;
            Mrph delta = morphs[morphIndex];
            
            basePos    += weight * delta.dPos;
            baseNormal += weight * delta.dNrml;
            baseTangent += weight * delta.dTan;
        }
    }

    // --- Skinning ---
    vec4 skinnedPos = vec4(0.0);
    vec3 skinnedNormal = vec3(0.0);
    vec3 skinnedTangent = vec3(0.0);

    uint boneCount = pConst.props1.x;

    if (boneCount == 0) { // No skinning
        skinnedPos = vec4(basePos, 1.0);
        skinnedNormal = baseNormal;
        skinnedTangent = baseTangent;
    } else {
        for (uint i = 0; i < 4; ++i) {
            uint id = inBoneIDs[i];

            float w = inBoneWs[i];
            mat4 boneMat = id < boneCount ? skinData[id] : mat4(1.0);

            skinnedPos     += w * (boneMat * vec4(basePos, 1.0));
            skinnedNormal  += w * mat3(boneMat) * baseNormal;
            skinnedTangent += w * mat3(boneMat) * baseTangent;
        }
    }

    vec4 worldPos = pConst.model * skinnedPos;

    gl_Position = glb.proj * glb.view * worldPos;

    fragWorldPos = worldPos.xyz;
    fragWorldNrml = normalize(mat3(pConst.model) * skinnedNormal);
    fragTexUV = vec2(inPos_Tu.w, inNrml_Tv.w);

    mat3 normalMat3 = transpose(inverse(mat3(glb.view * pConst.model)));
    fragTangent = vec4(normalize(mat3(pConst.model) * skinnedTangent), inTangent.w);
}
