#version 450

layout(binding = 0) uniform sampler2D opaqueSceneTexture;   // The opaque scene
layout(binding = 1) uniform sampler2D oitAccumTexture;      // OIT accumulation buffer
layout(binding = 2) uniform sampler2D oitRevealTexture;     // OIT reveal buffer

layout(location = 0) in vec2 fragTexCoord;
layout(location = 0) out vec4 outColor;

void main() {
    // Sample the opaque scene
    vec4 opaqueColor = texture(opaqueSceneTexture, fragTexCoord);
    
    // Sample OIT buffers
    vec4 accum = texture(oitAccumTexture, fragTexCoord);
    float reveal = texture(oitRevealTexture, fragTexCoord).r;
    
    // Composite transparent surfaces over opaque scene
    // This implements the final compositing step from the OIT paper
    
    // Handle division by zero
    float epsilon = 1e-5;
    vec3 transparentColor = accum.rgb / max(accum.a, epsilon);
    
    // Final blend: transparent over opaque
    // reveal = 1 - total_alpha, so (1 - reveal) gives us the coverage
    float transparency = 1.0 - reveal;
    
    outColor = vec4(
        transparentColor * transparency + opaqueColor.rgb * reveal,
        opaqueColor.a // Preserve opaque alpha
    );
}
