#ifndef UNPACK_ATTRIBUTES_GLSL_INCLUDED
#define UNPACK_ATTRIBUTES_GLSL_INCLUDED

// NOTE: .glsl extension is used for helper files with shader code

float3 decode_normal(uint encodedNormal)
{
    int x = (encodedNormal >> 0) & 0xFF;
    int y = (encodedNormal >> 8) & 0xFF;
    int z = (encodedNormal >> 16) & 0xFF;

    float3 normal;
    normal.x = (x / 255.0) * 2.0 - 1.0;
    normal.y = (y / 255.0) * 2.0 - 1.0;
    normal.z = (z / 255.0) * 2.0 - 1.0;

    return normal;
}

#endif // UNPACK_ATTRIBUTES_GLSL_INCLUDED
