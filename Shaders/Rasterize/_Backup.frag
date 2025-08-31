#version 450
#extension GL_EXT_nonuniform_qualifier : require      // for nonuniformEXT()
#extension GL_EXT_samplerless_texture_functions : enable // sometimes required by toolchains; optional

layout(set = 0, binding = 0) uniform GlobalUBO {
    mat4 proj;
    mat4 view;
    vec4 props; // General purpose: <float time>, <unused>, <unused>, <unused>
    vec4 cameraPos;    // xyz: camera position, w: fov
    vec4 cameraForward; // xyz: forward direction, w: aspect ratio
    vec4 cameraRight;   // xyz: camera right, w: near
    vec4 cameraUp;      // xyz: camera up, w: far
} glb;


struct Material {
    vec4 shadingParams; // <float shading>, <float toonLevel>, <float normalBlend>, <float unused>
    uvec4 texIndices;
};

layout(std430, set = 1, binding = 0) readonly buffer MaterialBuffer {
    Material materials[];
};

layout(set = 2, binding = 0) uniform sampler2D textures[];

layout(location = 0) in flat uvec4 fragProperties;
layout(location = 1) in vec4 fragMultColor;
layout(location = 2) in vec2 fragUV;
layout(location = 3) in vec3 fragWorldPos;
layout(location = 4) in vec3 fragWorldNrml;
layout(location = 5) in vec4 fragTangent;

layout(location = 0) out vec4 outColor;

// BORROWED
const float PI = 3.14159265359;
const float timeSpeed = 10000.0; // actual timeOfDay passed already scaled in app

// const vec3 skyDayZenith   = vec3(0.20, 0.45, 0.70);

const vec3 skyDayZenith   = vec3(0.05, 0.09, 0.24);
const vec3 skyNightZenith = vec3(0.05, 0.09, 0.24);

vec3 calculateSunDirection(float timeOfDay, float latitude) {
    float theta = 2.0 * PI * timeOfDay;
    float tilt = radians(latitude);

    float y = sin(theta);
    float r = cos(theta);
    float x = r * cos(tilt);
    float z = r * sin(tilt);

    return normalize(vec3(x, y, z));
}

float applyToonShading(float value, uint toonLevel) {
    // Convert toonLevel to float for calculations
    float level = float(toonLevel);
    
    // For level 0, return original value (no quantization)
    // For level > 0, apply quantization
    float bands = level + 1.0;
    float quantized = floor(value * bands + 0.5) / bands;
    
    // Use step function to select between original and quantized
    // step(1.0, level) returns 1.0 when level >= 1.0, 0.0 otherwise
    float useToon = step(1.0, level);
    float result = mix(value, quantized, useToon);
    
    return clamp(result, 0.0, 1.0);
}

void main() {
    Material material = materials[fragProperties.x];

    vec4 texColor = texture(textures[material.texIndices.x], fragUV);

    float discardThreshold = material.shadingParams.w;

    if (texColor.a < discardThreshold) { discard; }

    // BORROWED
    float time = glb.props.x * timeSpeed;

    // vec3 sunDir = calculateSunDirection(time, 45.0);

    vec3 sunDir = vec3(0.0, -1.0, 0.0);
    float sunElev = dot(sunDir, vec3(0.0, 1.0, 0.0));
    float elev01 = clamp((sunElev + 0.1) / 1.1, 0.0, 1.0); // smooth factor for time-of-day

    vec3 zenithCol  = mix(skyNightZenith, skyDayZenith,  elev01);

    // Fog effect
    float near = glb.cameraRight.w;
    float far = glb.cameraUp.w;
    
    float vertexDistance = length(fragWorldPos - glb.cameraPos.xyz);
    float fogMaxDistance = 69.0;
    float fogFactor = clamp((vertexDistance - fogMaxDistance) / fogMaxDistance, 0.0, 1.0);

    // Normal UV blending
    float normalBlend = material.shadingParams.z;
    vec3 normalColor = (fragWorldNrml + 1.0) * 0.5;

    // Normal mapping
    vec3 bitangent = cross(fragWorldNrml, fragTangent.xyz) * fragTangent.w;
    mat3 TBN = mat3(fragTangent.xyz, bitangent, fragWorldNrml);

    uint normalTexIndex = material.texIndices.y;
    vec3 mapN = texture(textures[normalTexIndex], fragUV).xyz * 2.0 - 1.0;

    // Use normal if no tangent is provided
    // vec3 mappedNormal = fragTangent.w == 0.0 ? fragWorldNrml : normalize(TBN * mapN);
    vec3 mappedNormal = fragWorldNrml;

    // S1mple shading
    int shading = int(material.shadingParams.x);
    float lightFactor = max(abs(dot(mappedNormal, -sunDir)), 1 - shading);
    lightFactor = length(mappedNormal) > 0.001 ? lightFactor : 1.0;

    uint toonLevel = uint(material.shadingParams.y);
    lightFactor = applyToonShading(lightFactor, toonLevel);
    lightFactor = 0.2 + lightFactor * 0.8; // Ambient

    // Atmospheric blending
    vec3 rgbColor = texColor.rgb + normalColor * normalBlend;
    vec3 rgbFinal = rgbColor * fragMultColor.rgb * zenithCol;
    rgbFinal = mix(rgbFinal * lightFactor, zenithCol * 0.2, fogFactor);

    // Combined alpha
    float alpha = texColor.a * fragMultColor.a;

    outColor = vec4(rgbFinal, alpha);
}
