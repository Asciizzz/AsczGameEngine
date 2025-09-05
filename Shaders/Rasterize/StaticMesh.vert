#version 450

layout(set = 0, binding = 0) uniform GlobalUBO {
    mat4 proj;
    mat4 view;
} glb;


layout(location = 0) in vec4 inPos_Tu;
layout(location = 1) in vec4 inNrml_Tv;
layout(location = 2) in vec4 inTangent;

// Instance data
layout(location = 3) in vec4 trformT_S;
layout(location = 4) in vec4 trformR;
layout(location = 5) in vec4 multColor;

layout(location = 0) out vec4 fragMultColor;
layout(location = 1) out vec2 fragUV;
layout(location = 2) out vec3 fragWorldPos;
layout(location = 3) out vec3 fragWorldNrml;
layout(location = 4) out vec4 fragTangent;


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

    vec3 worldPos = (rotMat * (inPos_Tu.xyz * scale)) + translation;
    gl_Position = glb.proj * glb.view * vec4(worldPos, 1.0);

    fragWorldPos = worldPos.xyz;
    fragUV = vec2(inPos_Tu.w, inNrml_Tv.w);
    fragMultColor = multColor;

    // Assume normalized by default
    vec3 newNormal = rotMat * inNrml_Tv.xyz;
    fragWorldNrml = newNormal;

    // Same thing
    fragTangent = vec4(rotMat * inTangent.xyz, inTangent.w);
}