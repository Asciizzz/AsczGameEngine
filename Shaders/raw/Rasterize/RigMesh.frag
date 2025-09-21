#version 450
#extension GL_EXT_nonuniform_qualifier : require      // for nonuniformEXT()
#extension GL_EXT_samplerless_texture_functions : enable // sometimes required by toolchains; optional

#include "../Common/lighting.glsl"

layout(push_constant) uniform PushConstant {
    uvec4 properties; // x = material index, y = light count, z = unused, w = unused
} pushConstant;

struct Material {
    vec4  shadingParams; // shadingFlag, toonLevel, normalBlend, discardThreshold
    uvec4 texIndices;    // albedo, normal, unused, unused
};

layout(std430, set = 1, binding = 0) readonly buffer MaterialBuffer {
    Material materials[];
};

layout(set = 2, binding = 0) uniform sampler2D textures[];

layout(std430, set = 4, binding = 0) readonly buffer LightBuffer {
    Light lights[];
};


layout(location = 0) in vec3 fragWorldPos;
layout(location = 1) in vec3 fragWorldNrml;
layout(location = 2) in vec2 fragTexUV;

layout(location = 0) out vec4 outColor;

float applyToonShading(float value, uint toonLevel) {
    float level = float(toonLevel);
    float bands = level + 1.0;
    float quantized = floor(value * bands + 0.5) / bands;
    float useToon = step(1.0, level);
    float result = mix(value, quantized, useToon);
    return clamp(result, 0.0, 1.0);
}

vec4 getTexture(uint texIndex, vec2 uv) {
    return texture(textures[nonuniformEXT(texIndex)], uv);
}

void main() {
    Material material = materials[pushConstant.properties.x];
    uint lightCount = pushConstant.properties.y;

    // Albedo texture - texIndices.x now contains albedo texture index
    uint albTexIndex = material.texIndices.x;
    vec4 texColor = getTexture(albTexIndex, fragTexUV);

    // Discard low opacity fragments
    float discardThreshold = material.shadingParams.w;
    if (texColor.a < discardThreshold) { discard; }

    // Ensure we have a valid normal
    vec3 normal = length(fragWorldNrml) > 0.001 ? normalize(fragWorldNrml) : vec3(0.0, 1.0, 0.0);

    // Check if shading is disabled
    bool shadingFlag = material.shadingParams.x > 0.5;
    
    vec3 finalColor;
    if (shadingFlag && lightCount > 0u) {
        // Use dynamic lighting system
        vec3 totalLighting = vec3(0.0);
        
        // Add ambient lighting
        totalLighting += texColor.rgb * 0.1;
        
        // Calculate contribution from each light
        for (uint i = 0u; i < lightCount; i++) {
            Light light = lights[i];
            uint lightType = uint(light.position.w);
            vec3 lightColor = light.color.rgb;
            float intensity = light.color.a;
            
            vec3 lightDir;
            float attenuation = 1.0;
            
            if (lightType == LIGHT_TYPE_DIRECTIONAL) {
                // Directional light
                lightDir = normalize(-light.direction.xyz);
            } else if (lightType == LIGHT_TYPE_POINT) {
                // Point light
                vec3 lightPos = light.position.xyz;
                lightDir = normalize(lightPos - fragWorldPos);
                float range = light.direction.w;
                float attenuationFactor = light.params.z;
                attenuation = calculateAttenuation(lightPos, fragWorldPos, range, attenuationFactor);
            } else if (lightType == LIGHT_TYPE_SPOT) {
                // Spot light
                vec3 lightPos = light.position.xyz;
                vec3 spotDir = normalize(light.direction.xyz);
                vec3 lightToFrag = fragWorldPos - lightPos;
                lightDir = normalize(lightPos - fragWorldPos);
                
                float range = light.direction.w;
                float attenuationFactor = light.params.z;
                float innerCone = light.params.x;
                float outerCone = light.params.y;
                
                attenuation = calculateAttenuation(lightPos, fragWorldPos, range, attenuationFactor);
                float spotFactor = calculateSpotFactor(spotDir, lightToFrag, innerCone, outerCone);
                attenuation *= spotFactor;
            }
            
            // Calculate diffuse lighting
            float NdotL = max(dot(normal, lightDir), 0.0);
            
            // Combine everything
            vec3 contribution = lightColor * intensity * NdotL * attenuation;
            totalLighting += contribution * texColor.rgb;
        }
        
        // Apply toon shading to the lighting result
        uint toonLevel = uint(material.shadingParams.y);
        float lightIntensity = length(totalLighting) / length(texColor.rgb);
        lightIntensity = applyToonShading(lightIntensity, toonLevel);
        
        finalColor = texColor.rgb * lightIntensity;
    } else {
        // No lighting - use full color
        finalColor = texColor.rgb;
    }

    outColor = vec4(finalColor, 1.0);
}
