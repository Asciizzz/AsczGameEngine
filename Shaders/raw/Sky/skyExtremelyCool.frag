#version 450

layout(set = 0, binding = 0) uniform GlobalUBO {
    mat4 proj;
    mat4 view;
    vec4 prop1;   // prop1.x = time
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

    // force into view-space direction
    eyeCoord = vec4(eyeCoord.xy, -1.0, 0.0);

    vec4 worldDir = invView * eyeCoord;
    return normalize(worldDir.xyz);
}

// --------------------------------------------------
// Noise functions (hash → noise → FBM)
// --------------------------------------------------
float hash(vec3 p) {
    p = fract(p * 0.3183099 + vec3(0.1, 0.2, 0.3));
    p *= 17.0;
    return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}

float noise(vec3 p) {
    vec3 i = floor(p);
    vec3 f = fract(p);

    f = f*f*(3.0 - 2.0*f);

    float v000 = hash(i + vec3(0,0,0));
    float v100 = hash(i + vec3(1,0,0));
    float v010 = hash(i + vec3(0,1,0));
    float v110 = hash(i + vec3(1,1,0));
    float v001 = hash(i + vec3(0,0,1));
    float v101 = hash(i + vec3(1,0,1));
    float v011 = hash(i + vec3(0,1,1));
    float v111 = hash(i + vec3(1,1,1));

    float x00 = mix(v000, v100, f.x);
    float x10 = mix(v010, v110, f.x);
    float x01 = mix(v001, v101, f.x);
    float x11 = mix(v011, v111, f.x);

    float y0 = mix(x00, x10, f.y);
    float y1 = mix(x01, x11, f.y);

    return mix(y0, y1, f.z);
}

float fbm(vec3 p) {
    float v = 0.0;
    float a = 0.5;
    for (int i = 0; i < 5; i++) {
        v += a * noise(p);
        p *= 2.0;
        a *= 0.5;
    }
    return v;
}

// --------------------------------------------------
// Cloud raymarch
// --------------------------------------------------

float hg(float cosTheta, float g)
{
    float g2 = g * g;
    return (1.0 / (4.0 * 3.14159)) *
           ((1.0 - g2) / pow(1.0 + g2 - 2.0 * g * cosTheta, 1.5));
}


vec3 renderClouds(vec3 rayDir, float time, vec3 cameraPos, vec3 sunDir)
{
    if (rayDir.y < 0.01) return vec3(0.0);

    const float CLOUD_ALT   = 15000.0;
    const float CLOUD_THICK = 200.0;
    const float STEP_SIZE   = 800.0;
    const int   STEPS       = 16;

    float t = CLOUD_ALT / max(rayDir.y, 0.001);

    vec3 col = vec3(0.0);
    float trans = 1.0;

    for (int i = 0; i < STEPS; i++)
    {
        float viewDist = t + float(i) * STEP_SIZE;
        vec3 worldPos = cameraPos + rayDir * viewDist;

        // Noise LOD
        float lod = clamp(viewDist * 0.00005, 0.0, 1.0);
        float scale = mix(0.00025, 0.00005, lod);

        float n = fbm(worldPos * scale + vec3(0.0, time * 0.00003, 0.0));
        float d = smoothstep(0.5, 0.75, n);

        // Distance density fade (fixes white sheet)
        float distFade = 1.0 / (1.0 + viewDist * 0.0001);
        d *= distFade;

        // Anisotropy reduces with distance
        float aniso = mix(0.6, 0.2, lod);
        float phase = hg(dot(rayDir, sunDir), aniso);

        float absorb = d * 0.25;
        float light = d * (0.7 + 0.5 * phase);

        col += trans * light;
        trans *= 1.0 - absorb;

        if (trans < 0.01) break;
    }

    return col;
}

// --------------------------------------------------
// Main
// --------------------------------------------------
void main() {
    float time = glb.prop1.x;
    vec3 rayDir = reconstructRayDirection();

    // Base sky gradient
    vec3 sky = mix(
        vec3(0.1, 0.2, 0.4),
        vec3(0.5, 0.7, 1.0),
        clamp(rayDir.y * 0.5 + 0.5, 0.0, 1.0)
    );

    // Volumetric cloud layer
    mat4 invView = inverse(glb.view);
    vec3 camPos = invView[3].xyz;

    vec3 sunDir = normalize(vec3(0.3, 1.0, 0.2)); // static sunlight

    vec3 clouds = renderClouds(rayDir, time, camPos, sunDir) * 0.5;

    vec3 finalColor = sky + clouds;

    // Ground (simple)
    if (rayDir.y < 0.0)
        finalColor = vec3(0.02);

    outColor = vec4(finalColor, 1.0);
}
