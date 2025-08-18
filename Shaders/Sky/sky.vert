#version 450

layout(set = 0,binding = 0) uniform GlobalUBO {
    mat4 proj;
    mat4 view;
} glb;


// Currently sky shader is sharing the same pipeline as the main rasterization shaders
// Before implementing actual sky shader rendering and stuff, we'll have to use these instead
layout(location = 0) in vec3 dummy0;
layout(location = 1) in vec3 dummy1;
layout(location = 2) in vec2 dummy2;
layout(location = 3) in vec3 dummy3;
layout(location = 4) in vec3 dummy4;
layout(location = 5) in vec4 dummy5;
layout(location = 6) in float dummy6;
layout(location = 7) in vec4 dummy7;

// Fullscreen quad vertices (no input needed)
vec2 positions[3] = vec2[](
    vec2(-1.0, -1.0),  // Bottom left
    vec2( 3.0, -1.0),  // Bottom right (extended)
    vec2(-1.0,  3.0)   // Top left (extended)
);

layout(location = 0) out vec2 fragScreenCoord;

void main() {
    vec2 pos = positions[gl_VertexIndex];
    gl_Position = vec4(pos, 0.0, 1.0);  // NDC coordinates
    fragScreenCoord = pos;  // Pass screen coordinates to fragment shader
}
