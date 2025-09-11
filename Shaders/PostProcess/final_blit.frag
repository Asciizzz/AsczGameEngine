#version 450

layout(location = 0) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform sampler2D uInput;

void main() {
    // Sample the final processed image
    vec3 color = texture(uInput, fragTexCoord).rgb;
    
    // Simple linear to sRGB conversion (approximate)
    color = pow(color, vec3(1.0/2.2));
    
    outColor = vec4(color, 1.0);
}
