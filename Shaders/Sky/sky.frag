#version 450

layout(binding = 0) uniform GlobalUBO {
    mat4 proj;
    mat4 view;
    vec4 prop1; // x = timeOfDay, rest unused
} glb;

layout(location = 0) in vec2 fragScreenCoord;
layout(location = 0) out vec4 outColor;

const float PI = 3.14159265359;
const float timeSpeed = 10000.0; // actual timeOfDay passed already scaled in app

// Gradients for day/night
// const vec3 skyDayZenith   = vec3(0.20, 0.45, 0.70);
const vec3 skyDayZenith = vec3(0.02, 0.03, 0.08);
const vec3 skyNightZenith = vec3(0.02, 0.03, 0.08);

// const vec3 skyDayHorizon  = vec3(0.95, 0.85, 0.55);
const vec3 skyDayHorizon  = vec3(0.08, 0.08, 0.12);
const vec3 skyNightHorizon= vec3(0.08, 0.08, 0.12);

// const vec3 seaDay   = vec3(0.05, 0.20, 0.30);
const vec3 seaDay   = vec3(0.02, 0.03, 0.08);
const vec3 seaNight = vec3(0.02, 0.03, 0.08);


// vec3 sunColorDay   = vec3(1.0, 0.95, 0.8);
// vec3 sunColorNight = vec3(0.3, 0.4, 0.6); // bluish moon-like glow
vec3 sunColorDay = vec3(0.0);
vec3 sunColorNight = vec3(0.0);

// --------------------------------------------------
// Sun direction calculation
// --------------------------------------------------
vec3 calculateSunDirection(float timeOfDay, float latitude) {
    float theta = 2.0 * PI * timeOfDay;
    float tilt = radians(latitude);

    float y = sin(theta);
    float r = cos(theta);
    float x = r * cos(tilt);
    float z = r * sin(tilt);

    return normalize(vec3(x, y, z));
}

// --------------------------------------------------
// Hash & Starfield generation
// --------------------------------------------------
float hash(vec2 p) {
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

float starField(vec2 uv, float time) {
    // Make stars drift slowly across the sky (same direction as sun)
    // Assume sun moves along +X in UV space — tweak speed as needed
    uv.x += time * 0.002; // drift rate
    uv.y += time * 0.0005; // optional slight vertical drift

    // Wrap UV to avoid visible jumps when it goes out of range
    uv = fract(uv);

    // Scale grid for finer resolution
    vec2 gridUV = uv * 800.0;

    // Find cell + local position in cell
    vec2 cell = floor(gridUV);
    vec2 local = fract(gridUV);

    // Randomness per cell
    float h = hash(cell);
    float star = step(0.897, h); // rarity threshold

    // If star exists in this cell, fade by distance from center
    if (star > 0.0) {
        vec2 center = vec2(0.5);
        float dist = length(local - center);
        float falloff = smoothstep(0.05, 0.0, dist); // soft edge
        return falloff * (0.8 + 0.2 * hash(cell + 1.23));
    }
    return 0.0;
}


// --------------------------------------------------
// Sky calculation
// --------------------------------------------------

vec3 calculateSkyColor(vec3 rayDir) {
    float time = glb.prop1.x * timeSpeed;

    vec3 sunDir = calculateSunDirection(time, 45.0);
    float sunElev = dot(sunDir, vec3(0.0, 1.0, 0.0));
    float elev01 = clamp((sunElev + 0.1) / 1.1, 0.0, 1.0); // smooth factor for time-of-day

    // Blend sky colors based on elevation
    vec3 horizonCol = mix(skyNightHorizon, skyDayHorizon, elev01);
    vec3 zenithCol  = mix(skyNightZenith,  skyDayZenith,  elev01);

    // Gradient between horizon and zenith
    float sky_t = clamp(rayDir.y * 2.2, 0.0, 1.0);
    float skyGradT = pow(sky_t, 0.35);
    vec3 skyGrad = mix(horizonCol, zenithCol, skyGradT);

    // Sun highlight
    float SdotR = max(0.0, dot(sunDir, rayDir));
    vec3 sunColor      = mix(sunColorNight, sunColorDay, elev01);
    float sunFocus = 1240.0;
    float sunIntensity = 100.0;
    float sun_t = pow(SdotR, sunFocus) * sunIntensity;
    vec3 sunIllumination = sunColor * sun_t;

    vec3 skyCol;
    if (rayDir.y > 0.0) {
        // Above horizon
        skyCol = skyGrad + sunIllumination;
    } else {
        // Below horizon → sea
        float seaBlend = clamp(rayDir.y * -1.0, 0.0, 1.0);
        vec3 sea = mix(seaNight, seaDay, elev01);
        skyCol = sea;
    }

    // --------------------------------------------------
    // Stars — only above horizon
    // --------------------------------------------------
    if (rayDir.y > 0.0) {
        // Darkness factor — stars fade at dusk/dawn
        // float darkness = clamp(1.0 - elev01 * 4.0, 0.0, 1.0);
        
        float darkness = 1.0;

        // Convert rayDir to sky UV
        float az = atan(rayDir.z, rayDir.x) / (2.0 * PI);
        az = fract(az); // wrap around
        float alt = rayDir.y * 0.5 + 0.5;
        vec2 uv = vec2(az, alt);

        float stars = starField(uv, time);
        stars *= pow(darkness, 4.0); // softer fade

        skyCol += vec3(stars);
    }

    return skyCol;
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
