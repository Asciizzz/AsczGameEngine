#version 450

layout(set = 0, binding = 0) uniform GlobalUBO {
    mat4 proj;
    mat4 view;
    // vec4 props; // General purpose: <float time>, <unused>, <unused>, <unused>
    // vec4 cameraPos;     // xyz = camera position, w = fov (radians)
    // vec4 cameraForward; // xyz = camera forward, w = aspect ratio  
    // vec4 cameraRight;   // xyz = camera right, w = near
    // vec4 cameraUp;      // xyz = camera up, w = far
} glb;

struct Material {
    vec4 shadingParams;
    ivec4 texIndices;
};

layout(std430, set = 1, binding = 0) buffer MaterialBuffer {
    Material materials[];
};

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
layout(location = 4) out float vertexLightFactor;  // Pre-computed lighting
layout(location = 5) out vec4 fragScreenPos;  // Screen space position for depth sampling

// Toon shading function (moved from fragment shader)
float applyToonShading(float value, int toonLevel) {
    // Convert toonLevel to float for calculations
    float level = float(toonLevel);
    
    // For level 0, return original value (no quantization)
    // For level > 0, apply quantization
    float bands = level + 1.0;
    float quantized = floor(value * bands + 0.5) / bands;
    
    // Use step function to select between original and quantized
    // step(1.0, level) returns 1.0 when level >= 1.0, 0.0 otherwise
    float useToon = step(1.0, level);
    float result = mix(value, quantized, useToon);
    
    return clamp(result, 0.0, 1.0);
}

void main() {
    mat4 modelMatrix = mat4(modelRow0, modelRow1, modelRow2, modelRow3);

    vec4 worldPos = modelMatrix * vec4(inPos, 1.0);
    fragWorldPos = worldPos.xyz;

    gl_Position = glb.proj * glb.view * worldPos;

    fragScreenPos = gl_Position;

    // Proper normal transformation that handles non-uniform scaling
    mat3 nrmlMat = transpose(inverse(mat3(modelMatrix)));
    vec3 worldNormal = normalize(nrmlMat * inNrml);
    fragWorldNrml = worldNormal;

    fragTxtr = inTxtr;
    fragInstanceColor = instanceColor;

    // === MOVED LIGHTING COMPUTATION FROM FRAGMENT TO VERTEX ===
    // Unwrap material properties
    Material material = materials[0];
    float shading = material.shadingParams.x;
    int toonLevel = int(material.shadingParams.y);

    vec3 lightDir = normalize(vec3(-1.0, -0.3, 1.0));
    
    // The idea is that if shading is disabled, lightFactor should be 1.0
    float lightFactor = max(dot(worldNormal, -lightDir), 1 - shading);
    lightFactor = length(worldNormal) > 0.001 ? lightFactor : 1.0;

    // Apply toon shading based on toon level
    lightFactor = applyToonShading(lightFactor, toonLevel);
    lightFactor = 0.2 + lightFactor * 0.8; // Ambient

    vertexLightFactor = lightFactor;
}