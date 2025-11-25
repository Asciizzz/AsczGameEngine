#version 450

layout(push_constant) uniform PushConstant {
    vec4 props;
} pConst;

layout(set = 0, binding = 0) uniform GlobalUBO {
    mat4 proj;
    mat4 view;
} glb;

layout(location = 0) in vec4  inPos_Tu;   // .xyz = pos, .w = u (if you use packed UVs)
layout(location = 1) in vec4  inNrml_Tv;  // .xyz = normal, .w = v (if packed)
layout(location = 2) in vec4  inTangent;  // .xyz = tangent, .w = handedness
layout(location = 3) in uvec4 inBoneIDs;
layout(location = 4) in vec4  inBoneWs;  

// Instance
// layout(location = 3) in vec4  mat4_0;
// layout(location = 4) in vec4  mat4_1;
// layout(location = 5) in vec4  mat4_2;
// layout(location = 6) in vec4  mat4_3;
// layout(location = 7) in uvec4 other; // .x = skinOffset, .y = skinCount

layout(location = 5) in vec4  mat4_0;
layout(location = 6) in vec4  mat4_1;
layout(location = 7) in vec4  mat4_2;
layout(location = 8) in vec4  mat4_3;
layout(location = 9) in uvec4 other;

layout(location = 0) out vec3 fragNrml;

void main() {
    mat4 model = mat4(mat4_0, mat4_1, mat4_2, mat4_3);

    vec4 worldPos4 = model * vec4(inPos_Tu.xyz, 1.0);

    mat3 normalMat = transpose(inverse(mat3(model)));
    fragNrml = normalMat * inNrml_Tv.xyz;

    gl_Position = glb.proj * glb.view * worldPos4;
}
