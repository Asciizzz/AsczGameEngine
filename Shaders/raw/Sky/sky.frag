#version 450

layout(set = 0, binding = 0) uniform GlobalUBO {
    mat4 proj;
    mat4 view;
    vec4 prop1; // prop1.x = time
} glb;

layout(location = 0) in vec2 fragScreenCoord;
layout(location = 0) out vec4 outColor;

// --------------------------------------------------
// Ray reconstruction
// --------------------------------------------------
vec3 reconstructRayDirection() {
    vec4 clipCoord = vec4(fragScreenCoord, 1.0, 1.0);
    mat4 invView = inverse(glb.view);
    mat4 invProj = inverse(glb.proj);
    vec4 eyeCoord = invProj * clipCoord;
    eyeCoord = vec4(eyeCoord.xy, -1.0, 0.0);
    vec4 worldCoord = invView * eyeCoord;
    return normalize(worldCoord.xyz);
}

// --------------------------------------------------
// Main
// --------------------------------------------------
void main() {
    float time = glb.prop1.x * 500.0; // Time scaling
    vec3 rayDir = reconstructRayDirection();

    vec3 baseSky = vec3(0.44, 0.59, 0.77);

    vec3 baseHorizon = vec3(0.5, 0, 0.5);
    float horizonFactor = smoothstep(-0.1, 0.2, rayDir.y);
    baseSky = mix(baseHorizon, baseSky, horizonFactor);

    // Below horizon
    if (rayDir.y < 0.0) baseSky = vec3(0.02, 0.02, 0.02);

    outColor = vec4(baseSky, 1.0);
}
