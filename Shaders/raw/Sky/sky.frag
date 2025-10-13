#version 450

layout(set = 0, binding = 0) uniform GlobalUBO {
    mat4 proj;
    mat4 view;
    vec4 prop1; // unused for simple sky
} glb;

layout(location = 0) in vec2 fragScreenCoord;
layout(location = 0) out vec4 outColor;

// Simple sky colors - bright editor-style sky
const vec3 skyTop    = vec3(0.4, 0.7, 1.0);    // Bright blue at zenith
const vec3 skyHorizon = vec3(0.8, 0.9, 1.0);   // Light blue/white at horizon
const vec3 groundColor = vec3(0.3, 0.3, 0.3);  // Gray ground

// --------------------------------------------------
// Sky calculation
// --------------------------------------------------

vec3 calculateSkyColor(vec3 rayDir) {
    if (rayDir.y > 0.0) {
        // Above horizon - sky gradient
        float t = clamp(rayDir.y, 0.0, 1.0);
        // Use power curve for more natural gradient
        t = pow(t, 0.4);
        return mix(skyHorizon, skyTop, t);
    } else {
        // Below horizon - ground
        return groundColor;
    }
}

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
    vec3 rayDir = reconstructRayDirection();
    vec3 skyColor = calculateSkyColor(rayDir);

    outColor = vec4(skyColor, 1.0);
}
