#version 450

layout(push_constant) uniform PushConstant {
    mat4 model;
    uvec4 props1; // .x = material index, .y = static flag (not used), .z = special value
} pConst;

layout(set = 0, binding = 0) uniform GlobalUBO {
    mat4 proj;
    mat4 view;
} glb;

layout(location = 0) in vec4  inPos_Tu;   // .xyz = pos, .w = u (if you use packed UVs)
layout(location = 1) in vec4  inNrml_Tv;  // .xyz = normal, .w = v (if packed)
layout(location = 2) in vec4  inTangent;  // .xyz = tangent, .w = handedness

layout(location = 0) out vec2 fragTexUV;
layout(location = 1) out vec3 fragWorldPos;
layout(location = 2) out vec3 fragWorldNrml;
layout(location = 3) out vec4 fragTangent;
layout(location = 4) out uint fragMaterialIndex;
layout(location = 5) out uint fragSpecial;

void main() {
    // world-space position
    vec4 worldPos4 = pConst.model * vec4(inPos_Tu.xyz, 1.0);
    fragWorldPos = worldPos4.xyz;

    mat3 normalMat = transpose(inverse(mat3(pConst.model)));

    fragWorldNrml = normalize(normalMat * inNrml_Tv.xyz);
    fragTangent = vec4(normalize(normalMat * inTangent.xyz), inTangent.w);
    fragTexUV = vec2(inPos_Tu.w, inNrml_Tv.w);

    fragMaterialIndex = pConst.props1.x;

    gl_Position = glb.proj * glb.view * worldPos4;

    fragSpecial = pConst.props1.z;
}
