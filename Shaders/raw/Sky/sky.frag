#version 450

layout(set = 0, binding = 0) uniform GlobalUBO {
    mat4 proj;
    mat4 view;
    vec4 prop1; // prop1.x = time
} glb;

layout(location = 0) in vec2 fragScreenCoord;
layout(location = 0) out vec4 outColor;

// --------------------------------------------------
// Sky Color Palette
// --------------------------------------------------
const vec3 nightZenith   = vec3(0.015, 0.02, 0.05);  // Deep indigo
const vec3 nightMid      = vec3(0.03, 0.04, 0.08);   // Slightly lighter
const vec3 nightHorizon  = vec3(0.06, 0.07, 0.12);   // Faint bluish horizon
const vec3 groundColor   = vec3(0.02, 0.02, 0.025);  // Very dark ground

const vec3 moonColor     = vec3(0.9, 0.9, 1.0);
const vec3 moonDir       = normalize(vec3(-1.0, 0.2, -1.0));
const float moonSize     = 0.005;
const float moonGlow     = 0.03;

// --------------------------------------------------
// Utility hash for randomness
// --------------------------------------------------
float hash(vec3 p) {
    p = fract(p * 0.3183099 + vec3(0.1, 0.2, 0.3));
    p += dot(p, p.yzx + 19.19);
    return fract(p.x * p.y * p.z * 95.733);
}

// --------------------------------------------------
// Procedural scattered star field
// --------------------------------------------------
float starField(vec3 rayDir, float time) {
    // Spherical coordinate mapping
    vec2 uv = vec2(atan(rayDir.z, rayDir.x) / (2.0 * 3.14159) + 0.5,
                   asin(rayDir.y) / 3.14159 + 0.5);
    uv *= 400.0; // Star density control

    // Random seed per region
    vec2 grid = floor(uv);
    float star = 0.0;

    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            vec2 cell = grid + vec2(x, y);
            vec3 seed = vec3(cell, 12.0);
            vec2 pos = fract(vec2(hash(seed), hash(seed + 1.0)));
            float size = hash(seed + 2.0) * 0.02 + 0.002;
            float brightness = smoothstep(size * 1.5, size, distance(fract(uv), pos));
            float twinkle = 0.5 + 0.5 * sin(time * 2.5 + hash(seed) * 12.0);
            star += brightness * twinkle;
        }
    }

    return star * 0.4; // Intensity control
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
    float time = glb.prop1.x * 500.0; // Time scaling
    vec3 rayDir = reconstructRayDirection();

    // Sky gradient: blend 3 layers
    float y = clamp(rayDir.y * 0.5 + 0.5, 0.0, 1.0);
    vec3 baseSky = mix(nightHorizon, nightMid, pow(y, 0.7));
    baseSky = mix(baseSky, nightZenith, pow(y, 1.6));

    // Moon glow
    float moonDot = dot(rayDir, moonDir);
    float moonDisk = smoothstep(1.0 - moonSize, 1.0 - moonSize * 0.5, moonDot);
    float moonHalo = pow(max(0.0, moonDot), 32.0);
    baseSky += moonColor * (moonDisk * 1.5 + moonHalo * 0.2);

    // Stars
    float stars = starField(rayDir, time);
    baseSky += vec3(stars);

    // Below horizon
    if (rayDir.y < 0.0)
        baseSky = groundColor;

    outColor = vec4(baseSky, 1.0);
}
