#version 450

// Push constants - this is the key part!
layout(push_constant) uniform PushDemo {
    vec4 color; // RGBA color that will be multiplied with the base color
} pushDemo;

// Input from vertex shader
layout(location = 0) in vec3 fragWorldPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;

// Descriptor sets
layout(set = 1, binding = 0) uniform MaterialUBO {
    vec4 albedo;
    vec4 materialParams; // metallic, roughness, etc.
} material;

layout(set = 2, binding = 0) uniform sampler2D textures[16];

// Output
layout(location = 0) out vec4 outColor;

void main() {
    // Base color from material
    vec4 baseColor = material.albedo;
    
    // Sample texture if available (assuming texture 0 is albedo)
    vec4 textureColor = texture(textures[0], fragTexCoord);
    
    // Combine base color with texture
    vec4 surfaceColor = baseColor * textureColor;
    
    // Apply push constant color multiplication - this is the demo!
    vec4 finalColor = surfaceColor * pushDemo.color;
    
    // Simple lighting calculation
    vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0)); // Fixed light direction
    float lightAmount = max(dot(normalize(fragNormal), lightDir), 0.2); // Ambient of 0.2
    
    // Apply lighting
    finalColor.rgb *= lightAmount;
    
    // Output final color
    outColor = finalColor;
}
