#version 450

layout(location = 0) in vec2 fragTexUV;
layout(location = 1) in vec3 fragWorldPos;
layout(location = 2) in vec3 fragWorldNrml;
layout(location = 3) in vec4 fragTangent;

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform sampler2D uAlbedo;
layout(set = 1, binding = 1) uniform sampler2D uNormal;

void main() {
    // Get texture color
    vec4 color = texture(uAlbedo, fragTexUV);
    if (color.a < 0.1) { discard; }

    outColor = color;
}
