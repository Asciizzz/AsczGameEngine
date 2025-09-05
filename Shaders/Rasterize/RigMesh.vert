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
    // Get the average inverse bind matrix for the bones
    mat4 skinMat4 = inWeights[0] * finalPose[inBoneID[0]] +
                    inWeights[1] * finalPose[inBoneID[1]] +
                    inWeights[2] * finalPose[inBoneID[2]] +
                    inWeights[3] * finalPose[inBoneID[3]];

    vec4 worldPos = skinMat4 * vec4(inPos_Tu.xyz, 1.0);
    gl_Position = glb.proj * glb.view * worldPos;

    // Transform normal correctly using inverse transpose of the upper 3x3 matrix
    mat3 normalMatrix = transpose(inverse(mat3(skinMat4)));
    vec3 normal = normalize(normalMatrix * inNrml_Tv.xyz);

    debugLight = dot(normal, normalize(vec3(1.0, 1.0, 1.0)));
    fragUV = vec2(inPos_Tu.w, inNrml_Tv.w);

}