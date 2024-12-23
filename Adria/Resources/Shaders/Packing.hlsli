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
	n /= (abs(n.x) + abs(n.y) + abs(n.z));
    n.xy = n.z >= 0.0 ? n.xy : (1.0 - abs(n.yx)) * lerp(-1.0, 1.0, n.xy >= 0.0);
    return n.xy;
}

float3 DecodeNormalOctahedron(float2 f)
{
	 float3 n = float3(f.x, f.y, 1.0 - abs(f.x) - abs(f.y));
    float t = saturate(-n.z);
    n.xy += lerp(t, -t, n.xy >= 0.0);
    return normalize(n);
}

uint EncodeNormal16x2(float3 n)
{
    float2 v = EncodeNormalOctahedron(n) * 0.5 + 0.5;
    uint2 u16 = (uint2)round(v * 65535.0);

    return (u16.x << 16) | u16.y;
}

float3 DecodeNormal16x2(uint f)
{
    uint2 u16 = uint2(f >> 16, f & 0xffff);
    float2 n = u16 / 65535.0;
    
    return DecodeNormalOctahedron(n * 2.0 - 1.0);
}


float4 EncodeGBufferNormalRT(float3 viewNormal, float metallic, uint shadingExtension)
{
    float2 encodedNormal = EncodeNormalOctahedron(viewNormal) * 0.5f + 0.5f;
    return float4(encodedNormal, metallic, (float)shadingExtension / 255.0f);
}

void DecodeGBufferNormalRT(float4 data, out float3 viewNormal, out float metallic, out uint shadingExtension)
{
    viewNormal = DecodeNormalOctahedron(data.xy * 2.0f - 1.0f);
    metallic = data.z;
    shadingExtension = uint(data.w * 255.0f);
}

//credit: https://github.com/zhaijialong/RealEngine/blob/main/shaders/common.hlsli 
float4 EncodeClearCoat(float clearCoat, float roughness, float3 normal)
{
    uint clearCoatU6 = (uint)round(clearCoat * 63.0);
    uint roughnessU6 = (uint)round(roughness * 63.0);

    float2 encodedNormal = EncodeNormalOctahedron(normal) * 0.5 + 0.5;
    uint2 normalU10 = (uint2)round(encodedNormal * 1023.0);
    
    uint4 dataU8 = uint4(clearCoatU6 | ((normalU10.x >> 2) & 0xC0),
                         roughnessU6 | ((normalU10.y >> 2) & 0xC0),
                         normalU10.x & 0xFF,
                         normalU10.y & 0xFF);
    
    return dataU8 / 255.0;
}
void DecodeClearCoat(float4 data, out float clearCoat, out float roughness, out float3 normal)
{
    uint4 dataU8 = (uint4)round(data * 255.0);

    uint clearCoatU6 = dataU8.x & 0x3F;
    uint roughnessU6 = dataU8.y & 0x3F;
    uint2 normalU10 = uint2(dataU8.z | ((dataU8.x & 0xC0) << 2), dataU8.w | ((dataU8.y & 0xC0) << 2));
    
    clearCoat = clearCoatU6 / 63.0;
    roughness = roughnessU6 / 63.0;
    normal = DecodeNormalOctahedron((normalU10 / 1023.0) * 2.0 - 1.0);
}


#endif