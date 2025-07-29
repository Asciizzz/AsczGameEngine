#version 450

// Global uniform buffer for camera matrices
layout(binding = 0) uniform GlobalUBO {
    mat4 proj;
    mat4 view;
} glb;

layout(binding = 1) uniform sampler2D txtrSmplr;

layout(location = 0) in vec2 fragUV;
layout(location = 1) in flat uint fragTextureIndex;
layout(location = 2) in float fragOpacity;

layout(location = 0) out vec4 outColor;

void main() {
    vec4 texColor = texture(txtrSmplr, fragUV);
    
    // Apply per-billboard opacity control
    texColor.a *= fragOpacity;
    
    // Discard fully transparent pixels to avoid depth sorting issues
    if (texColor.a < 0.01) {
        discard;
    }
    
    outColor = texColor;
}
