#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNrml;
layout(location = 2) in vec2 inTxtr;

// 4x4 is too large for a single instance, so we use 4 vec4s
// Each vec4 represents a row of the 4x4 model matrix
layout(location = 3) in vec4 modelRow0;  // Row 0: [m00, m01, m02, m03]
layout(location = 4) in vec4 modelRow1;  // Row 1: [m10, m11, m12, m13]
layout(location = 5) in vec4 modelRow2;  // Row 2: [m20, m21, m22, m23]
layout(location = 6) in vec4 modelRow3;  // Row 3: [m30, m31, m32, m33]

layout(location = 0) out vec2 fragTxtr;
layout(location = 1) out vec3 fragNrml;

void main() {
    // Reconstruct model matrix from instance data
    mat4 modelMatrix = mat4(modelRow0, modelRow1, modelRow2, modelRow3);

    gl_Position = ubo.proj * ubo.view * modelMatrix * vec4(inPos, 1.0);

    fragNrml = mat3(modelMatrix) * inNrml;
    fragTxtr = inTxtr;
}