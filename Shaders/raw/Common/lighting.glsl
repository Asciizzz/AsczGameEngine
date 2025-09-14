#ifndef LIGHTING_GLSL
#define LIGHTING_GLSL

// Light structure matching the C++ LightVK struct
struct Light {
    vec4 position;  // xyz = position, w = light type (0=directional, 1=point, 2=spot)
    vec4 color;     // rgb = color, a = intensity
    vec4 direction; // xyz = direction (for directional/spot), w = range
    vec4 params;    // x = inner cone angle, y = outer cone angle, z = attenuation, w = unused
};

// Light types
const uint LIGHT_TYPE_DIRECTIONAL = 0u;
const uint LIGHT_TYPE_POINT = 1u;
const uint LIGHT_TYPE_SPOT = 2u;

// Calculate lighting contribution from a single light (using index instead of direct Light parameter)
vec3 calculateLightContribution(uint lightIndex, vec3 fragPos, vec3 fragNormal, vec3 albedo) {
    // This function will be called from fragment shaders that have access to the lights buffer
    // The actual implementation will be in the fragment shader since it needs buffer access
    return vec3(0.0); // Placeholder - this won't be used
}

// Utility functions for lighting calculations
float calculateAttenuation(vec3 lightPos, vec3 fragPos, float range, float attenuationFactor) {
    float distance = length(lightPos - fragPos);
    if (distance > range) return 0.0;
    
    float attenuation = 1.0 / (1.0 + attenuationFactor * distance * distance);
    return attenuation;
}

float calculateSpotFactor(vec3 lightDir, vec3 lightToFrag, float innerCone, float outerCone) {
    float cosAngle = dot(normalize(lightDir), normalize(lightToFrag));
    float spotFactor = smoothstep(outerCone, innerCone, cosAngle);
    return spotFactor;
}

#endif // LIGHTING_GLSL