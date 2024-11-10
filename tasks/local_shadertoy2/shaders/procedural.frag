#version 430
#define PI 3.1415

layout(location = 0) out vec4 color;
float iTime = 100.0;

void main()
{
    vec2 uv = gl_FragCoord.xy / vec2(1280, 720);

    color = vec4(vec3(0.5 + 0.5*cos(iTime+uv.xyx+vec3(0,2,4))), 1.);
}
