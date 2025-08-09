#version 450

layout(binding = 0) uniform GlobalUBO {
    mat4 proj;
    mat4 view;
    vec4 cameraPos;     // xyz = camera position, w = fov (radians)
    vec4 cameraForward; // xyz = camera forward, w = aspect ratio  
    vec4 cameraRight;   // xyz = camera right, w = unused
    vec4 cameraUp;      // xyz = camera up, w = unused
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
