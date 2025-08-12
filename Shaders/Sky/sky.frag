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

layout (binding = 1) uniform sampler2D depthSampler;

// If Vulkan validation return warnings where
// vertex attributes are not being consumed
// this shader is to blame XD

layout(location = 0) in vec2 fragScreenCoord;  // Screen coordinates [-1, 1]
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
    vec3 groundColor = vec3(0.6098, 0.725, 0.986);

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

// Random function for noise (global scope)
float random(vec2 uv) {
    return fract(sin(dot(uv.xy, vec2(12.9898,78.233))) * 43758.5453123);
}

void main() {


    vec2 depthUV = fragScreenCoord * 0.5 + 0.5;
    vec3 rayDir = reconstructRayDirection();
    vec3 skyColor = calculateSkyColor(rayDir);

    // --- God Rays Implementation (adapted from demo) ---
    vec3 sunDir = normalize(vec3(-1.0, -0.3, 1.0));
    vec3 camPos = glb.cameraPos.xyz;
    vec3 sunPosWorld = camPos + sunDir * 100.0;
    vec4 sunView = glb.view * vec4(sunPosWorld, 1.0);
    vec4 sunClip = glb.proj * sunView;
    sunClip /= sunClip.w;
    vec2 sunScreen = sunClip.xy;
    vec2 sunUV = sunScreen * 0.5 + 0.5;

    // Parameters (tweak as needed)
    const int SAMPLE_COUNT = 100;
    float ray_length = 1.0;
    float ray_intensity = 120.0;
    float light_source_scale = 1.0;
    float light_source_feather = 0.7;
    float noise_strength = 0.2;

    // Radial blur
    vec2 dir = depthUV - sunUV;
    vec2 ratio = vec2(1.0, 1.0); // Assume square pixels for now
    vec2 dir2 = depthUV / ratio - sunUV / ratio;
    float light_rays = 0.0;
    vec2 uv2;
    float scale, l;
    for (int i = 0; i < SAMPLE_COUNT; i++) {
        scale = 1.0 - ray_length * (float(i) / float(SAMPLE_COUNT - 1));
        float d = texture(depthSampler, dir * scale + sunUV).r;
        l = 1.0 - d;
        uv2 = dir2 * scale * (light_source_scale * light_source_scale);
        l *= smoothstep(1.0, 0.999 - light_source_feather, dot(uv2, uv2) * 4.0);
        light_rays += l / float(SAMPLE_COUNT);
    }


    // Noise to reduce banding
    float n = 1.0 - random(depthUV) * noise_strength * smoothstep(0.999 - 1.25, 1.0, dot(dir2, dir2) * 2.0);
    light_rays *= n;

    // Fade based on view angle
    float d_angle = clamp(-dot(normalize(rayDir), normalize(sunDir)), 0.0, 1.0);
    light_rays *= d_angle;

    vec3 godRayColor = vec3(1.2, 1.1, 0.9) * light_rays * ray_intensity;

    godRayColor = clamp(godRayColor, 0.0, 1.0); // Ensure color is within valid range

    vec3 finalColor = skyColor + godRayColor;
    outColor = vec4(finalColor, 1.0);
}
