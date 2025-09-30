#version 450

layout(location = 0) in vec2 fragTexUV;
layout(location = 1) in vec3 fragWorldPos;
layout(location = 2) in vec3 fragWorldNrml;
layout(location = 3) in vec4 fragTangent;

layout(location = 0) out vec4 outColor;

void main() {
    float intensity = abs(dot(normalize(fragWorldNrml), vec3(0.0, 1.0, 0.0)));
    intensity = 0.5 + intensity * 0.5;

    outColor = vec4(vec3(intensity), 1.0);
}
