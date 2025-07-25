#version 450

layout(binding = 0) uniform UniformBufferObject {
    // Camera matrices
    mat4 view;
    mat4 proj;
    
    // Lighting data
    vec3 lightDirection;
    vec3 lightColor;
    vec3 ambientLight;
    vec3 cameraPosition;
    
    // Time and animation
    float time;
    float deltaTime;
    
    // Material properties
    vec3 fogColor;
    float fogDensity;
    float fogStart;
    float fogEnd;
    
    // Screen/viewport info
    vec2 screenResolution;
    float aspectRatio;
    
    // Custom parameters
    vec4 customParams;
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
layout(location = 2) out vec3 fragWorldPos;
layout(location = 3) out float fragTime;     // Pass time to fragment shader
layout(location = 4) out vec3 fragViewDir;   // View direction for lighting

void main() {
    // Reconstruct model matrix from instance data
    mat4 modelMatrix = mat4(modelRow0, modelRow1, modelRow2, modelRow3);

    // Calculate world position
    vec4 worldPos = modelMatrix * vec4(inPos, 1.0);
    fragWorldPos = worldPos.xyz;
    
    // Example: Apply wave animation using time
    vec3 animatedPos = inPos;
    animatedPos.y += sin(ubo.time * 2.0 + inPos.x) * ubo.customParams.x; // Use customParams.x as wave amplitude
    
    vec4 animatedWorldPos = modelMatrix * vec4(animatedPos, 1.0);
    fragWorldPos = animatedWorldPos.xyz;

    gl_Position = ubo.proj * ubo.view * animatedWorldPos;

    fragNrml = mat3(modelMatrix) * inNrml;
    fragTxtr = inTxtr;
    
    // Pass additional data to fragment shader
    fragTime = ubo.time;
    fragViewDir = normalize(ubo.cameraPosition - fragWorldPos);
}