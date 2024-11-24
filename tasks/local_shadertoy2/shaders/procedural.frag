#version 430
#define PI 3.1415

layout(location = 0) out vec4 color;
layout(push_constant) uniform PushConstants {
    vec2 resolution;
    vec2 mouse;
    float time;
} pc;

void main()
{
    vec2 uv = gl_FragCoord.xy / pc.resolution;

    color = vec4(vec3(0.5 + 0.5*cos(pc.time + uv.xyx + vec3(0,2,4))), 1.);
}
