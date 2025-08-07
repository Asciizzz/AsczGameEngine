#version 450

layout(binding = 0) uniform GlobalUBO {
    mat4 proj;
    mat4 view;
} glb;

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNrml;
layout(location = 2) in vec2 inTxtr;

layout(location = 3) in vec4 modelRow0;  // Row 0: [m00, m01, m02, m03]
layout(location = 4) in vec4 modelRow1;  // Row 1: [m10, m11, m12, m13]
layout(location = 5) in vec4 modelRow2;  // Row 2: [m20, m21, m22, m23]
layout(location = 6) in vec4 modelRow3;  // Row 3: [m30, m31, m32, m33]
layout(location = 7) in vec4 instanceColor; // Instance color multiplier

layout(location = 0) out vec2 fragTxtr;
layout(location = 1) out vec3 fragWorldNrml;
layout(location = 2) out vec3 fragWorldPos;
layout(location = 3) out vec4 fragInstanceColor;

void main() {
    mat4 modelMatrix = mat4(modelRow0, modelRow1, modelRow2, modelRow3);

    vec4 worldPos = modelMatrix * vec4(inPos, 1.0);
    fragWorldPos = worldPos.xyz;

    gl_Position = glb.proj * glb.view * worldPos;

    // Proper normal transformation that handles non-uniform scaling
    mat3 nrmlMat = transpose(inverse(mat3(modelMatrix)));
    fragWorldNrml = normalize(nrmlMat * inNrml);

    fragTxtr = inTxtr;
    fragInstanceColor = instanceColor;
}