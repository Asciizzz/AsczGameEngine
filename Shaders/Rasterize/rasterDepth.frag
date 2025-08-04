#version 450

layout(binding = 0) uniform GlobalUBO {
    mat4 proj;
    mat4 view;
} glb;

layout(location = 0) in vec2 fragTxtr;
layout(location = 1) in vec3 fragWorldNrml;
layout(location = 2) in vec3 fragWorldPos;
layout(location = 3) in vec4 fragInstanceColor;

layout(location = 0) out vec4 outColor;

void main() {
    if (fragInstanceColor.a < 0.001) discard;

    // Extract camera position from view matrix
    // For a view matrix, the camera position is: -transpose(rotation) * translation
    // But we can get it more simply from the inverse view matrix translation
    mat4 invView = inverse(glb.view);
    vec3 cameraPos = invView[3].xyz;
    
    // Calculate distance from camera to fragment
    float distance = length(cameraPos - fragWorldPos);
    
    // Normalize distance to create depth gradient
    // Adjust these values based on your scene scale
    float nearPlane = 1.0;   // Objects closer than this will be black
    float farPlane = 100.0;  // Objects farther than this will be white
    
    float normalizedDepth = clamp((distance - nearPlane) / (farPlane - nearPlane), 0.0, 1.0);
    
    outColor = vec4(normalizedDepth);
}