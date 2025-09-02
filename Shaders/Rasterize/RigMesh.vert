#version 450
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_samplerless_texture_functions : enable

layout(set = 0, binding = 0) uniform GlobalUBO {
    mat4 proj;
    mat4 view;
} glb;

layout(std430, set = 3, binding = 0) readonly buffer BoneBuffer {
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

// Helper: convert hue (0..360) to RGB, brightness scaled by weight
vec3 hueToRGB(float hue, float weight) {
    float c = weight;
    float x = c * (1.0 - abs(mod(hue / 60.0, 2.0) - 1.0));
    float m = 0.0;

    if (hue < 60.0) return vec3(c, x, m);
    if (hue < 120.0) return vec3(x, c, m);
    if (hue < 180.0) return vec3(m, c, x);
    if (hue < 240.0) return vec3(m, x, c);
    if (hue < 300.0) return vec3(x, m, c);
    return vec3(c, m, x);
}

// Main conversion
vec3 boneWeightsToColor(vec4 weights) {
    vec3 color = vec3(0.0);

    color += hueToRGB(0.0,     weights.x);  // Bone 0 -> Red hue
    color += hueToRGB(120.0,   weights.y);  // Bone 1 -> Green hue
    color += hueToRGB(240.0,   weights.z);  // Bone 2 -> Blue hue
    color += hueToRGB(60.0,    weights.w);  // Bone 3 -> Yellow hue

    return clamp(color, 0.0, 1.0);           // Ensure RGB in [0,1]
}


void main() {
    // Get the average inverse bind matrix for the bones
    mat4 avgInverseBindMatrix = mat4(0.0);
    for (int i = 0; i < 4; ++i) {
        avgInverseBindMatrix += inverseBindMatrices[inBoneID[i]];
    }
    avgInverseBindMatrix /= 4.0;

    // vec4 worldPos = avgInverseBindMatrix * vec4(inPos_Tu.xyz, 1.0);
    vec4 worldPos = vec4(inPos_Tu.xyz, 1.0);

    vec3 normal = inNrml_Tv.xyz;

    debugLight = dot(normal, normalize(vec3(0.0, 1.0, 1.0)));

    debugColor = vec4(boneWeightsToColor(inWeights), 1.0);

    fragUV = vec2(inPos_Tu.w, inNrml_Tv.w);

    gl_Position = glb.proj * glb.view * worldPos;
}