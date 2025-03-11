#version 430
#define PI 3.1415
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "UniformParams.h"


layout(location = 0) out vec4 color;
layout(binding = 0, set = 0) uniform AppData
{
  UniformParams params;
};

void main()
{
    vec2 uv = gl_FragCoord.xy / params.resolution;

    color = vec4(vec3(0.5 + 0.5*cos(params.time + uv.xyx + vec3(0,2,4))), 1.);
}
