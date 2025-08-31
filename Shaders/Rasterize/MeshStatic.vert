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
layout(location = 3) in uvec4 properties;
layout(location = 4) in vec4 trformT_S;
layout(location = 5) in vec4 trformR;
layout(location = 6) in vec4 multColor;

layout(location = 0) out flat uvec4 fragProperties;
layout(location = 1) out vec4 fragMultColor;
layout(location = 2) out vec2 fragTxtr;
layout(location = 3) out vec3 fragWorldPos;
layout(location = 4) out vec3 fragWorldNrml;
layout(location = 5) out vec4 fragTangent;


void main() {
    // Extract translation and scale
    vec3 translation = trformT_S.xyz;
    float scale = trformT_S.w;

    // Extract rotation quaternion
    vec4 q = trformR; // x, y, z, w
    // Convert quat to rotation matrix
    mat3 rotMat = mat3(
        1.0 - 2.0*q.y*q.y - 2.0*q.z*q.z,  2.0*q.x*q.y - 2.0*q.z*q.w,      2.0*q.x*q.z + 2.0*q.y*q.w,
        2.0*q.x*q.y + 2.0*q.z*q.w,        1.0 - 2.0*q.x*q.x - 2.0*q.z*q.z,  2.0*q.y*q.z - 2.0*q.x*q.w,
        2.0*q.x*q.z - 2.0*q.y*q.w,        2.0*q.y*q.z + 2.0*q.x*q.w,      1.0 - 2.0*q.x*q.x - 2.0*q.y*q.y
    );

    // Construct model matrix
    mat4 modelMatrix = mat4(
        vec4(rotMat[0] * scale, 0.0),
        vec4(rotMat[1] * scale, 0.0),
        vec4(rotMat[2] * scale, 0.0),
        vec4(translation, 1.0)
    );

    vec4 worldPos = modelMatrix * vec4(inPos_Tu.xyz, 1.0);

    gl_Position = glb.proj * glb.view * worldPos;

    fragProperties = properties;
    fragWorldPos = worldPos.xyz;
    fragTxtr = vec2(inPos_Tu.w, inNrml_Tv.w);
    fragMultColor = multColor;

    // Proper normal transformation that handles non-uniform scaling
    mat3 nrmlMat = transpose(inverse(mat3(modelMatrix)));

    vec3 transNormal = normalize(nrmlMat * inNrml_Tv.xyz);
    fragWorldNrml = transNormal;

    fragTangent = inTangent;
}