#version 450

layout(set = 0, binding = 0) uniform GlobalUBO {
    mat4 proj;
    mat4 view;
    vec4 prop1; // unused for simple sky
} glb;

layout(location = 0) in vec2 fragScreenCoord;
layout(location = 0) out vec4 outColor;

// Natural sky colors - clear day sky
const vec3 skyTop     = vec3(0.1, 0.4, 0.8);   // Deep blue at zenith
const vec3 skyHorizon = vec3(0.6, 0.8, 0.95);  // Light blue at horizon
const vec3 groundColor = vec3(0.2, 0.2, 0.2); // Darker ground

// Sun properties
const vec3 sunDirection = normalize(vec3(1.0)); // Static sun position
const vec3 sunColor = vec3(1.0, 0.8, 0.7); // To offset the sky zenith
const float sunSize = 0.01;
const float sunIntensity = 3.0;

// --------------------------------------------------
// Sky calculation
// --------------------------------------------------

vec3 calculateSkyColor(vec3 rayDir) {
    if (rayDir.y > 0.0) {
        // Above horizon - sky gradient
        float t = clamp(rayDir.y, 0.0, 1.0);
        // Use power curve for more natural gradient
        t = pow(t, 0.4);
        vec3 skyColor = mix(skyHorizon, skyTop, t);
        
        // Add sun
        float sunDot = dot(rayDir, sunDirection);
        float sunMask = smoothstep(1.0 - sunSize, 1.0 - sunSize * 0.5, sunDot);
        
        // Sun glow effect
        float sunGlow = pow(max(0.0, sunDot), 32.0) * 0.3;
        
        // Combine sky, sun disk, and glow
        skyColor += sunColor * sunIntensity * sunMask;
        skyColor += sunColor * sunGlow;
        
        return skyColor;
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
