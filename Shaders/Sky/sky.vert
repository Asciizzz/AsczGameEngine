#version 450

layout(set = 0,binding = 0) uniform GlobalUBO {
    mat4 proj;
    mat4 view;
} glb;

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
