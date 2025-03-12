#ifndef UNPACK_ATTRIBUTES_GLSL_INCLUDED
#define UNPACK_ATTRIBUTES_GLSL_INCLUDED

// NOTE: .glsl extension is used for helper files with shader code

vec3 decode_normal(uint encodedNormal)
{
    uint x = (encodedNormal >> 0) & 0xFF;
    uint y = (encodedNormal >> 8) & 0xFF;
    uint z = (encodedNormal >> 16) & 0xFF;

    ivec3 normal = ivec3(x, y, z);

    return (normal / 255.0) * 2.0 - 1.0;
}

#endif // UNPACK_ATTRIBUTES_GLSL_INCLUDED
