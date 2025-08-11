#version 450

layout(binding = 0) uniform GlobalUBO {
    mat4 proj;
    mat4 view;
    // vec4 cameraPos;    // xyz: camera position, w: fov
    // vec4 cameraForward; // xyz: forward direction, w: aspect ratio
    // vec4 cameraRight;
    // vec4 cameraUp;
    // vec4 nearFar;       // x = near, y = far, z = unused, w = unused
} glb;

layout(binding = 1) uniform sampler2D txtrSmplr;

// Material uniform buffer
layout(binding = 2) uniform MaterialUBO {
    vec4 prop1; // <bool shading>, <int toonLevel>, <float normalBlend>, <float discardThreshold>
} material;

// TODO: Implement actual working depth sampler, not this piece of shi
// layout(binding = 3) uniform sampler2D depthSampler;

layout(location = 0) in vec2 fragTxtr;
layout(location = 1) in vec3 fragWorldNrml;
layout(location = 2) in vec3 fragWorldPos;
layout(location = 3) in vec4 fragInstanceColor;
layout(location = 4) in float vertexLightFactor;
layout(location = 5) in vec4 fragScreenPos;

layout(location = 0) out vec4 outColor;

void main() {
    vec4 texColor = texture(txtrSmplr, fragTxtr);
    float discardThreshold = material.prop1.w;

    if (texColor.a < discardThreshold) { discard; }

    vec3 screenCoords = fragScreenPos.xyz / fragScreenPos.w;
    vec2 depthUV = screenCoords.xy * 0.5 + 0.5; // Convert from NDC [-1,1] to UV [0,1]

    // float near = glb.nearFar.x;
    // float far = glb.nearFar.y;
    
    // World space depth
    // float z = sampledDepth * 2.0 - 1.0; // Back to NDC
    // float linearDepth = (2.0 * near * far) / (far + near - z * (far - near));

    float linearDepth = 1.0; // Placeholder before I actually fixed the depth

    float maxFogDistance = 70.0;
    float fogFactor = clamp(linearDepth / maxFogDistance, 0.0, 1.0);
    fogFactor = smoothstep(0.0, 1.0, fogFactor);

    vec3 skyColor = vec3(0.3098, 0.525, 0.686);

    float normalBlend = material.prop1.z;
    vec3 normal = normalize(fragWorldNrml);
    vec3 normalColor = (normal + 1.0) * 0.5;

    vec3 rgbColor = texColor.rgb + normalColor * normalBlend;
    vec3 rgbFinal = rgbColor * fragInstanceColor.rgb * skyColor;
    
    rgbFinal = mix(rgbFinal * vertexLightFactor, skyColor, fogFactor);
    
    float alpha = texColor.a * fragInstanceColor.a;

    outColor = vec4(rgbFinal, alpha);
}
