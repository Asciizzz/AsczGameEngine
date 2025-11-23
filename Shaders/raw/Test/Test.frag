#version 450

layout(location = 0) in vec3 fragNrml;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 lightDir = vec3(1.0, 1.0, 1.0);

    outColor = vec4(1.0) * abs(dot(normalize(fragNrml), normalize(lightDir)));
}
