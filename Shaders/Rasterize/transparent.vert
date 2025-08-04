#version 450

// Vertex attributes from the VBO
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec3 inNormal;

// Instance attributes from the instance buffer
layout(location = 3) in mat4 inInstanceMatrix;    // locations 3, 4, 5, 6
layout(location = 7) in vec4 inInstanceColor;     // location 7

// Global uniform buffer for camera matrices
layout(binding = 0) uniform GlobalUBO {
    mat4 proj;
    mat4 view;
} glb;

// Output to fragment shader
layout(location = 0) out vec2 fragTxtr;
layout(location = 1) out vec3 fragWorldNrml;
layout(location = 2) out vec3 fragWorldPos;
layout(location = 3) out vec4 fragInstanceColor;

void main() {
    // Transform vertex position to world space using instance matrix
    vec4 worldPos = inInstanceMatrix * vec4(inPosition, 1.0);
    
    // Transform to clip space
    gl_Position = glb.proj * glb.view * worldPos;
    
    // Pass texture coordinates
    fragTxtr = inTexCoord;
    
    // Transform normal to world space (assuming uniform scaling)
    fragWorldNrml = mat3(inInstanceMatrix) * inNormal;
    
    // Pass world position for lighting calculations
    fragWorldPos = worldPos.xyz;
    
    // Pass instance color (includes alpha for transparency)
    fragInstanceColor = inInstanceColor;
}
