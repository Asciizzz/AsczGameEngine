#version 450

layout(push_constant) uniform PushConstant {
    uvec4 data0;
    uvec4 data1;
    uvec4 data2;
} pConst;

/* pConst explanation:

data0 {
    .x = vertexFlag: {
        VERTEX_FLAG_NONE    = 0, (also means static mesh)
        VERTEX_FLAG_RIG     = 1 << 0,
        VERTEX_FLAG_MORPH   = 1 << 1,
        VERTEX_FLAG_COLOR   = 1 << 2
    }
    .y = vertexCount
    .z = morphTargetCount - number of morph targets
    .w = material index
}

data1 {
    .x = staticOffset (you could think of it as gl_VertexOffset)
    .y = rigOffset
    .z = colorOffset
    .w = mrphDltsOffset // Delta offsets
}

data2 {
    .x = mrphWsCount // Morph Weights count
    .y = reserved
    .z = reserved
    .w = reserved
}

}

*/

bool vHasSkin()  { return (pConst.data0.x & 1) != 0; }
bool vHasMorph() { return (pConst.data0.x & 2) != 0; }
bool vHasColor() { return (pConst.data0.x & 4) != 0; }

uint staticOffset()   { return pConst.data1.x; }
uint rigOffset()      { return pConst.data1.y; }
uint colorOffset()    { return pConst.data1.z; }
uint mrphDltsOffset() { return pConst.data1.w; }

layout(location = 0) in vec4  inPos_Tu;
layout(location = 1) in vec4  inNrml_Tv;
layout(location = 2) in vec4  inTangent;

layout(location = 3) in vec4  model4_0;
layout(location = 4) in vec4  model4_1;
layout(location = 5) in vec4  model4_2;
layout(location = 6) in vec4  model4_3;
layout(location = 7) in uvec4 rtData; // .x = skinOffset, .y = skinCount, .z = mrphWsOffset, .w = mrphWsCount

layout(location = 0) out vec3 fragWorld;
layout(location = 1) out vec3 fragNrml;
layout(location = 2) out vec2 fragUV;
layout(location = 3) out vec3 fragTangent;
layout(location = 4) out vec4 fragColor;

layout(set = 0, binding = 0) uniform GlobalUBO {
    mat4 proj;
    mat4 view;
} glb;

struct Rig {
    uvec4 boneIDs;
    vec4  boneWs;
};

struct Color {
    vec4 color;
};

struct Mrph {
    vec3 dPos;
    vec3 dNrml;
    vec3 dTang;
};

// Vertex extension buffers
layout (std430, set = 1, binding = 0) readonly buffer RigBuffer { Rig rigs[]; };
layout (std430, set = 1, binding = 1) readonly buffer ColorBuffer { Color colors[]; };
layout (std430, set = 1, binding = 2) readonly buffer MrphDltsBuffer { Mrph mrphDlts[]; };

// Runtime data buffers
layout (std430, set = 3, binding = 0) readonly buffer SkinBuffer { mat4 skinData[];};
layout (std430, set = 4, binding = 0) readonly buffer MrphWsBuffer { float mrphWs[]; };

Rig getRig() {
    return rigs[rigOffset() - staticOffset() + gl_VertexIndex];
}

void main() {
    mat4 model = mat4(model4_0, model4_1, model4_2, model4_3);

    vec3 basePos     = inPos_Tu.xyz;
    vec3 baseNormal  = inNrml_Tv.xyz;
    vec3 baseTangent = inTangent.xyz;

// ----------------------------------

    uint vertexCount = pConst.data0.y;
    uint vertexIdx   = gl_VertexIndex;

    uint mrphTargetCount = pConst.data0.z;

    uint mrphWsCount = rtData.w;
    if (mrphWsCount > 0 && vertexCount > 0) {
        uint mrphWsOffset = rtData.z;

        for (uint m = 0; m < mrphWsCount; ++m) {
            float weight = mrphWs[mrphWsOffset + m];

            if (abs(weight) < 0.0001) continue; // negligible

            uint morphIndex = m * vertexCount + vertexIdx;
            Mrph delta = mrphDlts[morphIndex + mrphDltsOffset()];

            basePos    += weight * delta.dPos;
            baseNormal += weight * delta.dNrml;
            baseTangent += weight * delta.dTang;
        }
    }

// ----------------------------------

    vec4 skinnedPos = vec4(0.0);
    vec3 skinnedNormal = vec3(0.0);
    vec3 skinnedTangent = vec3(0.0);

    uint skinCount = rtData.y;
    if (skinCount > 0 && vertexCount > 0) {
        uint skinOffset = rtData.x;
        Rig rig = getRig();

        for (uint i = 0; i < 4; ++i) {
            uint id = rig.boneIDs[i] + skinOffset;
            float w = rig.boneWs[i];
            mat4 skinMat = skinData[id];

            skinnedPos     += w * (skinMat * vec4(basePos, 1.0));
            skinnedNormal  += w * mat3(skinMat) * baseNormal;
            skinnedTangent += w * mat3(skinMat) * baseTangent;
        }
    } else  { // No skinning
        skinnedPos = vec4(basePos, 1.0);
        skinnedNormal = baseNormal;
        skinnedTangent = baseTangent;
    }

// ----------------------------------

    vec4 worldPos4 = model * skinnedPos;
    fragWorld = worldPos4.xyz;

    mat3 normalMat = transpose(inverse(mat3(model)));
    fragNrml = normalMat * skinnedNormal;
    fragUV = inPos_Tu.zw;
    fragTangent = skinnedTangent;

    fragColor = vHasColor() ? colors[gl_VertexIndex + colorOffset()].color : vec4(1.0);

    gl_Position = glb.proj * glb.view * worldPos4;
}
