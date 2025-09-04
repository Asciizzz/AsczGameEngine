#version 450

// Vertex input
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inTangent;
layout(location = 3) in vec2 inTexCoord;

// Instance input (if using instanced rendering)
layout(location = 4) in vec4 instancePos;
layout(location = 5) in vec4 instanceRot;
layout(location = 6) in float instanceScale;

// Global UBO
layout(set = 0, binding = 0) uniform GlobalUBO {
    mat4 view;
    mat4 proj;
    mat4 viewProj;
    vec4 viewPos;
    vec4 time;
} global;

// Output to fragment shader
layout(location = 0) out vec3 fragWorldPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragTexCoord;

void main() {
    // Calculate model matrix from instance data
    mat4 translation = mat4(1.0);
    translation[3] = vec4(instancePos.xyz, 1.0);
    
    mat4 scale = mat4(instanceScale);
    scale[3][3] = 1.0;
    
    // Simple rotation (assuming instanceRot is a quaternion)
    mat4 rotation = mat4(1.0); // For simplicity, not implementing full quaternion rotation here
    
    mat4 modelMatrix = translation * rotation * scale;
    
    // Transform position
    vec4 worldPos = modelMatrix * vec4(inPosition, 1.0);
    gl_Position = global.viewProj * worldPos;
    
    // Pass data to fragment shader
    fragWorldPos = worldPos.xyz;
    fragNormal = normalize(mat3(modelMatrix) * inNormal);
    fragTexCoord = inTexCoord;
}
