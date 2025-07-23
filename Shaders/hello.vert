#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) in vec3 inP;
layout(location = 1) in vec3 inN;
layout(location = 2) in vec2 inT;

// Instance data - model matrix from instance buffer
layout(location = 3) in vec4 instanceMatrix0;
layout(location = 4) in vec4 instanceMatrix1;
layout(location = 5) in vec4 instanceMatrix2;
layout(location = 6) in vec4 instanceMatrix3;

layout(location = 0) out vec2 fragT;

void main() {
    // Reconstruct model matrix from instance data
    mat4 modelMatrix = mat4(instanceMatrix0, instanceMatrix1, instanceMatrix2, instanceMatrix3);
    
    gl_Position = ubo.proj * ubo.view * modelMatrix * vec4(inP, 1.0);

    fragT = inT;
}