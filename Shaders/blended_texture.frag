#version 450

layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

void main() {
    // Blend vertex colors with texture (multiply RGB, preserve alpha)
    vec4 texColor = texture(texSampler, fragTexCoord);
    outColor = vec4(fragColor * texColor.rgb, 1.0);
}
