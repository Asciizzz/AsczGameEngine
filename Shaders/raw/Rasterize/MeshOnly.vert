#version 450

layout(push_constant) uniform PushConstant {
    mat4 model;
    uvec4 props1;
} pConst;

layout(set = 0, binding = 0) uniform GlobalUBO {
    mat4 proj;
    mat4 view;
} glb;

layout(location = 0) in vec4  inPos_Tu;   // .xyz = pos, .w = u (if you use packed UVs)
layout(location = 1) in vec4  inNrml_Tv;  // .xyz = normal, .w = v (if packed)
layout(location = 2) in vec4  inTangent;  // .xyz = tangent, .w = handedness

layout(location = 0) out vec3 fragNrml; // only need normal

void main() {
    vec4 worldPos4 = pConst.model * vec4(inPos_Tu.xyz, 1.0);

    mat3 normalMat = transpose(inverse(mat3(pConst.model)));
    fragNrml = normalMat * inNrml_Tv.xyz;

    gl_Position = glb.proj * glb.view * worldPos4;
}
