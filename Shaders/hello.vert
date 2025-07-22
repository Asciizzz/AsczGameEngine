#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) in vec3 inP;
layout(location = 1) in vec3 inN;
layout(location = 2) in vec2 inT;

layout(location = 0) out vec2 fragT;

void main() {
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inP, 1.0);

    fragT = inT;
}