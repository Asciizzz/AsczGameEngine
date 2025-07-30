#version 450

layout(binding = 0) uniform GlobalUBO {
    mat4 proj;
    mat4 view;
} glb;

// Billboard instance data
layout(location = 3) in vec3 billboardPos;
layout(location = 4) in float billboardWidth;
layout(location = 5) in float billboardHeight;
layout(location = 6) in uint textureIndex;
layout(location = 7) in vec2 uvMin;
layout(location = 8) in vec2 uvMax;
layout(location = 9) in vec4 color; // Color multiplier (RGBA)

layout(location = 0) out vec2 fragUV;
layout(location = 1) out flat uint fragTextureIndex;
layout(location = 2) out vec4 fragColorMult;

void main() {
    // Generate quad vertices for billboard (triangle strip)
    // Triangle strip order: bottom-left, bottom-right, top-left, top-right
    vec2 vertices[4] = vec2[4](
        vec2(-0.5, -0.5), // Bottom-left
        vec2( 0.5, -0.5), // Bottom-right
        vec2(-0.5,  0.5), // Top-left
        vec2( 0.5,  0.5)  // Top-right
    );
    
    vec2 localPos = vertices[gl_VertexIndex];
    
    // Scale by billboard dimensions
    localPos.x *= billboardWidth;
    localPos.y *= billboardHeight;
    
    // Get camera right and up vectors from view matrix
    vec3 cameraRight = vec3(glb.view[0][0], glb.view[1][0], glb.view[2][0]);
    vec3 cameraUp = vec3(glb.view[0][1], glb.view[1][1], glb.view[2][1]);
    
    // Billboard world position - always facing camera
    vec3 worldPos = billboardPos + cameraRight * localPos.x + cameraUp * localPos.y;
    
    gl_Position = glb.proj * glb.view * vec4(worldPos, 1.0);
    
    // Interpolate UV coordinates based on vertex position (corrected for triangle strip)
    vec2 uvCoords[4] = vec2[4](
        vec2(uvMin.x, uvMax.y), // Bottom-left
        vec2(uvMax.x, uvMax.y), // Bottom-right
        vec2(uvMin.x, uvMin.y), // Top-left
        vec2(uvMax.x, uvMin.y)  // Top-right
    );
    
    fragUV = uvCoords[gl_VertexIndex];
    fragTextureIndex = textureIndex;
    fragColorMult = color;
}
