#version 450

layout(push_constant) uniform PushConstant {
    mat4 model;
    uvec4 props1; // .x = material index
} transform;

layout(set = 0, binding = 0) uniform GlobalUBO {
    mat4 proj;
    mat4 view;
} glb;

layout(std430, set = 1, binding = 0) readonly buffer SkinBuffer {
    mat4 skinData[];
};

layout(location = 0) in vec4  inPos_Tu;   // .xyz = pos, .w = u (if you use packed UVs)
layout(location = 1) in vec4  inNrml_Tv;  // .xyz = normal, .w = v (if packed)
layout(location = 2) in vec4  inTangent;  // .xyz = tangent, .w = handedness
layout(location = 3) in uvec4 inBoneIDs;  // .xyzw = bone indices
layout(location = 4) in vec4  inWeights;  // .xyzw = bone weights

layout(location = 0) out vec2 fragTexUV;
layout(location = 1) out vec3 fragWorldPos;
layout(location = 2) out vec3 fragWorldNrml;
layout(location = 3) out vec4 fragTangent;
layout(location = 4) out uint fragMaterialIndex;

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
    vec3 skinnedTangent = vec3(0.0);

    for (uint i = 0; i < 4; ++i) {
        uint id = inBoneIDs[i];
        float w = inWeights[i];
        mat4 boneMat = skinData[id];

        skinnedPos     += w * (boneMat * vec4(basePos, 1.0));
        skinnedNormal  += w * mat3(boneMat) * baseNormal;
        skinnedTangent += w * mat3(boneMat) * inTangent.xyz;
    }

    vec4 worldPos = transform.model * skinnedPos;

    gl_Position = glb.proj * glb.view * worldPos;

    fragWorldPos = worldPos.xyz;
    fragWorldNrml = normalize(mat3(transform.model) * skinnedNormal);
    fragTexUV = vec2(inPos_Tu.w, inNrml_Tv.w);

    mat3 normalMat3 = transpose(inverse(mat3(glb.view * transform.model)));
    fragTangent = vec4(normalize(mat3(transform.model) * skinnedTangent), inTangent.w);

    fragMaterialIndex = transform.props1.x;

}
