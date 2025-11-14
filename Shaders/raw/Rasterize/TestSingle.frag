#version 450

layout(location = 0) in vec2 fragTexUV;
layout(location = 1) in vec3 fragWorldPos;
layout(location = 2) in vec3 fragWorldNrml;
layout(location = 3) in vec4 fragTangent;

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform MatProps {
    vec4 baseColor;
} uMaterial;

layout(set = 1, binding = 1) uniform sampler2D uAlbedo;
layout(set = 1, binding = 2) uniform sampler2D uNormal;
layout(set = 1, binding = 3) uniform sampler2D uMetallic;
layout(set = 1, binding = 4) uniform sampler2D uEmissive;

void main() {
    // Simple shader
    vec3 lightDir = normalize(vec3(-0.2, 0.1, -0.1));

    float nDot = dot(fragWorldNrml, lightDir);
    float intensity = 0.8 + clamp(nDot, 0.0, 1.0) * 0.2;

    vec4 baseColor = uMaterial.baseColor;

    vec4 albColor = texture(uAlbedo, fragTexUV);
    vec4 nrmlColor = texture(uNormal, fragTexUV);
    vec4 metalColor = texture(uMetallic, fragTexUV);
    vec4 emisColor = texture(uEmissive, fragTexUV);

    // Dont ask questions
    vec4 color = albColor * baseColor + emisColor;
    color.rgb *= intensity;

    if (color.a < 0.5) { discard; }

    outColor = color;
}
