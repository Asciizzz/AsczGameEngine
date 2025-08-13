#version 450

layout(binding = 0) uniform GlobalUBO {
    mat4 proj;
    mat4 view;
    vec4 cameraPos;     // xyz = camera position, w = fov (radians)
    vec4 cameraForward; // xyz = camera forward, w = aspect ratio  
    vec4 cameraRight;   // xyz = camera right, w = unused
    vec4 cameraUp;      // xyz = camera up, w = unused
    vec4 nearFar;       // x = near, y = far, z = unused, w = unused
} glb;

layout(location = 0) in vec2 fragScreenCoord;
layout(location = 0) out vec4 outColor;

// Sky calculation function (your original path tracer algorithm)
vec3 calculateSkyColor(vec3 rayDir) {
    vec3 sunDir = normalize(vec3(-1.0, -0.3, 1.0));

    // vec3 skyZenith = vec3(0.8, 0.6, 0.2);
    // vec3 skyHorizon = vec3(1.0, 0.8, 0.5);
    // vec3 groundColor = vec3(1.0, 0.8, 0.5);

    // Custom color
    vec3 skyZenith = vec3(0.2098, 0.425, 0.586);
    vec3 skyHorizon = vec3(1.0, 0.9, 0.4);
    vec3 groundColor = vec3(0.1098, 0.225, 0.986);

    vec3 sunColor = vec3(0.3, 0.2, 0.1);

    float sunFocus = 1240.0;
    float sunIntensity = 100.0;
    
    float sky_t = clamp(rayDir.y * 2.2, 0.0, 1.0);
    float skyGradT = pow(sky_t, 0.35);
    vec3 skyGrad = mix(skyHorizon, skyZenith, skyGradT);

    // Sun calculation
    float SdotR = dot(sunDir, rayDir);
    SdotR = max(0.0, -SdotR);
    float sun_t = pow(SdotR, sunFocus) * sunIntensity;
    vec3 sunIllumination = sunColor * sun_t;

    // Sky mask (above/below horizon)
    bool sky_mask = rayDir.y > 0.0;

    return sky_mask ? skyGrad + sunIllumination : groundColor;
}

// Reconstruct ray direction from screen coordinates
vec3 reconstructRayDirection() {
    // Method 1: Using inverse view-projection matrix (more robust)
    vec4 clipCoord = vec4(fragScreenCoord, 1.0, 1.0);
    
    // Transform to world space using inverse view-projection
    mat4 invView = inverse(glb.view);
    mat4 invProj = inverse(glb.proj);
    
    // Transform from clip space to eye space
    vec4 eyeCoord = invProj * clipCoord;
    eyeCoord = vec4(eyeCoord.xy, -1.0, 0.0);  // Point far into the scene
    
    // Transform from eye space to world space  
    vec4 worldCoord = invView * eyeCoord;
    
    // Calculate ray direction from camera to world point
    vec3 rayDir = normalize(worldCoord.xyz);
    
    return rayDir;
}

void main() {
    vec3 rayDir = reconstructRayDirection();
    vec3 skyColor = calculateSkyColor(rayDir);

    outColor = vec4(skyColor, 1.0);
}
