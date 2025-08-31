#version 450
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_samplerless_texture_functions : enable

layout(set = 0, binding = 0) uniform GlobalUBO {
    mat4 proj;
    mat4 view;
    // vec4 props; // General purpose: <float time>, <unused>, <unused>, <unused>
    // vec4 cameraPos;     // xyz = camera position, w = fov (radians)
    // vec4 cameraForward; // xyz = camera forward, w = aspect ratio  
    // vec4 cameraRight;   // xyz = camera right, w = near
    // vec4 cameraUp;      // xyz = camera up, w = far
} glb;


layout(location = 0) in vec4 inPos_Tu;
layout(location = 1) in vec4 inNrml_Tv;
layout(location = 2) in vec4 inTangent;

// Instance data
layout(location = 3) in uvec4 properties; // <materialIndex>, <indicator>, <empty>, <empty>
layout(location = 4) in vec4 mat4_row0;  // Row 0: [m00, m01, m02, m03]
layout(location = 5) in vec4 mat4_row1;  // Row 1: [m10, m11, m12, m13]
layout(location = 6) in vec4 mat4_row2;  // Row 2: [m20, m21, m22, m23]
layout(location = 7) in vec4 mat4_row3;  // Row 3: [m30, m31, m32, m33]
layout(location = 8) in vec4 instanceColor; // Instance color multiplier

layout(location = 0) out flat uvec4 fragProperties;
layout(location = 1) out vec4 fragInstanceColor;
layout(location = 2) out vec2 fragTxtr;
layout(location = 3) out vec3 fragWorldPos;
layout(location = 4) out vec3 fragWorldNrml;
layout(location = 5) out vec4 fragTangent;


void main() {
    mat4 modelMatrix = mat4(mat4_row0, mat4_row1, mat4_row2, mat4_row3);
    vec4 worldPos = modelMatrix * vec4(inPos_Tu.xyz, 1.0);

    gl_Position = glb.proj * glb.view * worldPos;

    fragProperties = properties;
    fragWorldPos = worldPos.xyz;
    fragTxtr = vec2(inPos_Tu.w, inNrml_Tv.w);
    fragInstanceColor = instanceColor;

    // Proper normal transformation that handles non-uniform scaling
    mat3 nrmlMat = transpose(inverse(mat3(modelMatrix)));

    vec3 transNormal = normalize(nrmlMat * inNrml_Tv.xyz);
    fragWorldNrml = transNormal;

    fragTangent = inTangent;
}