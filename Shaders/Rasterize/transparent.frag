#version 450

// Global uniform buffer for camera matrices
layout(binding = 0) uniform GlobalUBO {
    mat4 proj;
    mat4 view;
} glb;

layout(binding = 1) uniform sampler2D txtrSmplr;

// Material uniform buffer (reserved for future material properties)
layout(binding = 2) uniform MaterialUBO {
    float padding;
} material;

layout(location = 0) in vec2 fragTxtr;
layout(location = 1) in vec3 fragWorldNrml;
layout(location = 2) in vec3 fragWorldPos;
layout(location = 3) in vec4 fragInstanceColor;

// OIT render targets
layout(location = 0) out vec4 accumBuffer;  // Weighted color accumulation
layout(location = 1) out float revealBuffer; // Coverage

void main() {
    vec3 lightPos = vec3(0.0, 100.0, 0.0);

    vec3 lightDir = normalize(lightPos - fragWorldPos);
    vec3 normal = normalize(fragWorldNrml);

    float lightFactor = max(dot(normal, lightDir), 0.0);
    // In case there's no normal
    lightFactor = length(fragWorldNrml) > 0.001 ? lightFactor : 1.0;

    // Final brightness factor
    float finalFactor = 0.1 + lightFactor * 0.9;

    // Sample texture
    vec4 texColor = texture(txtrSmplr, fragTxtr);
    
    // Apply instance color multiplication
    vec4 finalColor = texColor * fragInstanceColor;
    
    // Apply lighting
    finalColor.rgb *= finalFactor;
    
    // Use instance alpha (multColor.w) combined with texture alpha for transparency
    // The instance color already contains the alpha we want to use
    float combinedAlpha = fragInstanceColor.a * texColor.a;
    finalColor.a = combinedAlpha;
    
    // Discard fully transparent pixels (optimization)
    if (finalColor.a < 0.001) {
        discard;
    }
    
    // Calculate depth in camera space (z-coordinate after view transform)
    vec4 viewPos = glb.view * vec4(fragWorldPos, 1.0);
    float z = -viewPos.z; // Negative because view space Z points towards camera
    
    // Weight function from the OIT paper - tuned for depth range and alpha values
    // Using weight equation 9 from the paper
    float weight = max(min(1.0, max(max(finalColor.r, finalColor.g), finalColor.b) * finalColor.a), finalColor.a) *
                   clamp(0.03 / (1e-5 + pow(z / 200.0, 4.0)), 1e-2, 3e3);
    
    // Output premultiplied alpha with weight
    accumBuffer = vec4(finalColor.rgb * finalColor.a, finalColor.a) * weight;
    
    // Output coverage (1 - alpha for proper blending)
    revealBuffer = finalColor.a;
}
