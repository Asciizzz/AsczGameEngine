#version 450

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstants {
    float iTime;
    vec2 iResolution;
} pc;

float sdf(in vec3 pos) {
    pos = mod(pos, 10.0);
    return length(pos - vec3(5.0)) - 1.0;
}

void main() {
    vec2 fragCoord = gl_FragCoord.xy;
    vec2 uv = (fragCoord * 2.0 - pc.iResolution.xy) / max(pc.iResolution.x, pc.iResolution.y);

    vec3 origin = vec3(0.0, 5.0, 0.0) * pc.iTime;
    float angle = radians(pc.iTime * 3.0);
    uv *= mat2(cos(angle), -sin(angle), sin(angle), cos(angle));
    
    vec3 ray_dir = normalize(vec3(sin(uv.x), cos(uv.x) * cos(uv.y), sin(uv.y)));
    vec3 ray_pos = origin;
    
    float ray_length = 0.0;
    
    for (float i = 0.0; i < 7.0; i++) {
        float dist = sdf(ray_pos);
        ray_length += dist;
        ray_pos += ray_dir * dist;
        ray_dir = normalize(ray_dir + vec3(uv.x, 0.0, uv.y) * dist * 0.3);
    }
    
    vec3 o = vec3(sdf(ray_pos));
    o = cos(o + vec3(6.0, 0.0, 0.5));
    o *= smoothstep(38.0, 20.0, ray_length);

    outColor = vec4(o, 1.0);
}
