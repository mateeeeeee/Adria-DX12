#ifndef _PACKING_
#define _PACKING_

float2 UnpackHalf2(in uint packed)
{
    float2 unpacked;
    unpacked.x = f16tof32(packed.x);
    unpacked.y = f16tof32(packed.x >> 16);
    return unpacked;
}

float4 UnpackHalf4(in uint2 packed)
{
    float4 unpacked;
    unpacked.x = f16tof32(packed.x);
    unpacked.y = f16tof32(packed.x >> 16);
    unpacked.z = f16tof32(packed.y);
    unpacked.w = f16tof32(packed.y >> 16);
    return unpacked;
}

float3 UnpackHalf3(in uint2 packed)
{
    float3 unpacked;
    unpacked.x = f16tof32(packed.x);
    unpacked.y = f16tof32(packed.x >> 16);
    unpacked.z = f16tof32(packed.y);
    return unpacked;
}

uint2 UnpackTwoUint16FromUint32(in uint packed)
{
    uint2 unpacked;
    unpacked.x = uint((packed >> 16) & 0xffff);
    unpacked.y = uint(packed & 0xffff);
    return unpacked;
}


float4 UnpackUintColor(uint color)
{
    float4 outCol = float4((color >> 24 & 0xff) / 255.0, (color >> 16 & 0xff) / 255.0f, (color >> 8 & 0xff) / 255.0, (color >> 0 & 0xff) / 255.0);
    return outCol;
}

float2 OctWrap(float2 v)
{
	return float2((1.0f - abs(v.y)) * (v.x >= 0.0f ? 1.0f : -1.0f), (1.0f - abs(v.x)) * (v.y >= 0.0f ? 1.0f : -1.0f));
}

float2 EncodeNormalOctahedron(float3 n)
{
	float2 p = float2(n.x, n.y) * (1.0f / (abs(n.x) + abs(n.y) + abs(n.z)));
	p = (n.z < 0.0f) ? OctWrap(p) : p;
	return p;
}

float3 DecodeNormalOctahedron(float2 p)
{
	float3 n = float3(p.x, p.y, 1.0f - abs(p.x) - abs(p.y));
	float2 tmp = (n.z < 0.0f) ? OctWrap(float2(n.x, n.y)) : float2(n.x, n.y);
	n.x = tmp.x;
	n.y = tmp.y;
	return normalize(n);
}


#endif