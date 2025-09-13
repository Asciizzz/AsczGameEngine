#version 450

layout(set = 0, binding = 0) uniform GlobalUBO {
    mat4 proj;
    mat4 view;
} glb;


layout(location = 0) in vec4 inPos_Tu;
layout(location = 1) in vec4 inNrml_Tv;
layout(location = 2) in vec4 inTangent;

layout(location = 0) out vec4 fragMultColor;
layout(location = 1) out vec2 fragTexUV;
layout(location = 2) out vec3 fragWorldPos;
layout(location = 3) out vec3 fragWorldNrml;
layout(location = 4) out vec4 fragTangent;


void main() {
    gl_Position = glb.proj * glb.view * vec4(inPos_Tu.xyz, 1.0);

    fragWorldPos = inPos_Tu.xyz;
    fragTexUV = vec2(inPos_Tu.w, inNrml_Tv.w);
    fragMultColor = vec4(1.0);

    fragWorldNrml = inNrml_Tv.xyz;

    // Same thing
    fragTangent = vec4(mat3(1.0) * inTangent.xyz, inTangent.w);
}