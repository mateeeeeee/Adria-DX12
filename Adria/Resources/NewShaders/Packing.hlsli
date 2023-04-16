#pragma once

static float2 UnpackHalf2(in uint packed)
{
    float2 unpacked;
    unpacked.x = f16tof32(packed.x);
    unpacked.y = f16tof32(packed.x >> 16);
    return unpacked;
}

static float4 UnpackHalf4(in uint2 packed)
{
    float4 unpacked;
    unpacked.x = f16tof32(packed.x);
    unpacked.y = f16tof32(packed.x >> 16);
    unpacked.z = f16tof32(packed.y);
    unpacked.w = f16tof32(packed.y >> 16);
    return unpacked;
}

static float3 UnpackHalf3(in uint2 packed)
{
    float3 unpacked;
    unpacked.x = f16tof32(packed.x);
    unpacked.y = f16tof32(packed.x >> 16);
    unpacked.z = f16tof32(packed.y);
    return unpacked;
}

static float4 UnpackUintColor(uint color)
{
    float4 outCol = float4((color >> 24 & 0xff) / 255.0, (color >> 16 & 0xff) / 255.0f, (color >> 8 & 0xff) / 255.0, (color >> 0 & 0xff) / 255.0);
    return outCol;
}