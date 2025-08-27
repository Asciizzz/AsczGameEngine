#version 450

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
    vec4 shadingParams;
    ivec4 texIndices;
};

layout(std430, set = 1, binding = 0) readonly buffer MaterialBuffer {
    Material materials[];
};

#extension GL_EXT_nonuniform_qualifier : require      // for nonuniformEXT()
#extension GL_EXT_samplerless_texture_functions : enable // sometimes required by toolchains; optional

layout(set = 2, binding = 0) uniform sampler2D textures[]; // runtime-sized array (descriptor indexing)

layout(location = 0) in vec2 fragTxtr;
layout(location = 1) in vec3 fragWorldPos;
layout(location = 2) in vec3 fragWorldNrml;
layout(location = 3) in flat ivec4 fragProperties;
layout(location = 4) in vec4 fragInstanceColor;
layout(location = 5) in float vertexLightFactor;

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

void main() {
    Material material = materials[fragProperties.x];

    vec4 texColor = texture(textures[material.texIndices.x], fragTxtr);

    float discardThreshold = material.shadingParams.w;

    if (texColor.a < discardThreshold) { discard; }

    // BORROWED
    float time = glb.props.x * timeSpeed;

    vec3 sunDir = calculateSunDirection(time, 45.0);
    float sunElev = dot(sunDir, vec3(0.0, 1.0, 0.0));
    float elev01 = clamp((sunElev + 0.1) / 1.1, 0.0, 1.0); // smooth factor for time-of-day

    vec3 zenithCol  = mix(skyNightZenith,  skyDayZenith,  elev01);

    float near = glb.cameraRight.w;
    float far = glb.cameraUp.w;
    
    float vertexDistance = length(fragWorldPos - glb.cameraPos.xyz);
    float fogMaxDistance = 69.0;
    float fogFactor = clamp((vertexDistance - fogMaxDistance) / fogMaxDistance, 0.0, 1.0);

    float normalBlend = material.shadingParams.z;
    vec3 normal = normalize(fragWorldNrml);
    vec3 normalColor = (normal + 1.0) * 0.5;

    vec3 rgbColor = texColor.rgb + normalColor * normalBlend;
    vec3 rgbFinal = rgbColor * fragInstanceColor.rgb * zenithCol;

    rgbFinal = mix(rgbFinal * vertexLightFactor, zenithCol * 0.2, fogFactor);

    float alpha = texColor.a * fragInstanceColor.a;

    outColor = vec4(rgbFinal, alpha);
}
