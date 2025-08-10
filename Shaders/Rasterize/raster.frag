#version 450

layout(binding = 0) uniform GlobalUBO {
    mat4 proj;
    mat4 view;
    vec4 cameraPos;    // xyz: camera position, w: fov
    vec4 cameraForward; // xyz: forward direction, w: aspect ratio
    vec4 cameraRight;
    vec4 cameraUp;
} glb;

layout(binding = 1) uniform sampler2D txtrSmplr;

// Material uniform buffer
layout(binding = 2) uniform MaterialUBO {
    vec4 prop1; // <bool shading>, <int toonLevel>, <float normalBlend>, <float discardThreshold>
} material;

layout(location = 0) in vec2 fragTxtr;
layout(location = 1) in vec3 fragWorldNrml;
layout(location = 2) in vec3 fragWorldPos;
layout(location = 3) in vec4 fragInstanceColor;
layout(location = 4) in float vertexLightFactor;

layout(location = 0) out vec4 outColor;

void main() {
    vec4 texColor = texture(txtrSmplr, fragTxtr);
    float discardThreshold = material.prop1.w;

    if (texColor.a < discardThreshold) { discard; }

    float distance = length(fragWorldPos - glb.cameraPos.xyz);
    
    float maxFogDistance = 60.0;
    float fogFactor = clamp(distance / maxFogDistance, 0.0, 1.0);
    
    fogFactor = smoothstep(0.0, 1.0, fogFactor);

    vec3 skyColor = vec3(0.8, 0.6, 0.2);

    float normalBlend = material.prop1.z;
    vec3 normal = normalize(fragWorldNrml);
    vec3 normalColor = (normal + 1.0) * 0.5;

    vec3 tintColor = vec3(0.8, 0.6, 0.2);

    vec3 rgbColor = texColor.rgb + normalColor * normalBlend;
    vec3 rgbFinal = rgbColor * fragInstanceColor.rgb * tintColor;
    
    rgbFinal = mix(rgbFinal * vertexLightFactor, skyColor, fogFactor);
    
    float alpha = texColor.a * fragInstanceColor.a;

    outColor = vec4(rgbFinal, alpha * fogFactor);
}
