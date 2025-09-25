#version 450

layout(set = 0, binding = 0) uniform GlobalUBO {
    mat4 proj;
    mat4 view;
} glb;

layout(location = 0) in vec4 inPos_Tu;   // .xyz = pos, .w = u (if you use packed UVs)
layout(location = 1) in vec4 inNrml_Tv;  // .xyz = normal, .w = v (if packed)
layout(location = 2) in vec4 inTangent;  // .xyz = tangent, .w = handedness
layout(location = 3) in uvec4 inJoints; // .xyzw = joint indices (if skinned)
layout(location = 4) in vec4 inWeights;  // .xyzw = joint

layout(location = 0) out vec4 fragMultColor;
layout(location = 1) out vec2 fragTexUV;
layout(location = 2) out vec3 fragWorldPos;
layout(location = 3) out vec3 fragWorldNrml;
layout(location = 4) out vec4 fragTangent;

// build scale matrix (column-major constructor)
mat4 scaleMat(float s) {
    return mat4(
        s, 0.0, 0.0, 0.0,
        0.0, s, 0.0, 0.0,
        0.0, 0.0, s, 0.0,
        0.0, 0.0, 0.0, 1.0
    );
}

// rotation around X (safe, explicit, column-major)
mat4 rotateXMat(float deg) {
    float r = radians(deg);
    float c = cos(r);
    float s = sin(r);
    return mat4(
        1.0, 0.0,  0.0, 0.0,   // column 0
        0.0,  c,   s,   0.0,   // column 1
        0.0, -s,   c,   0.0,   // column 2
        0.0, 0.0,  0.0, 1.0    // column 3
    );
}

void main() {
    // construct model matrix properly instead of mutating a single mat
    mat4 R = rotateXMat(0.0);
    mat4 S = scaleMat(0.05);

    mat4 model = S * R;

    // world-space position
    vec4 worldPos4 = model * vec4(inPos_Tu.xyz, 1.0);
    fragWorldPos = worldPos4.xyz;

    // normal matrix = transpose(inverse(mat3(model))) (correct for non-uniform scales too)
    mat3 normalMat = transpose(inverse(mat3(model)));

    // transform and normalize normal & tangent
    fragWorldNrml = normalize(normalMat * inNrml_Tv.xyz);
    fragTangent = vec4(normalize(normalMat * inTangent.xyz), inTangent.w);

    // keep your packed UV convention if that's intentional:
    fragTexUV = vec2(inPos_Tu.w, inNrml_Tv.w);

    fragMultColor = vec4(1.0);

    gl_Position = glb.proj * glb.view * worldPos4;
}
