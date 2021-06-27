#include "../Globals/GlobalsCS.hlsli"

Texture2D<float4> inputTexture : register(t0);
RWTexture2D<float4> outputTexture : register(u0);



[numthreads(32,32, 1)]
void main(uint3 groupID : SV_GroupID, uint3 groupThreadID : SV_GroupThreadID, uint groupIndex : SV_GroupIndex, uint3 dispatchID : SV_DispatchThreadID)
{
    float2 uv = dispatchID.xy;

    float3 color = inputTexture[dispatchID.xy].rgb;

    //float intensity = dot(color.xyz, float3(0.2126f, 0.7152f, 0.0722f));
    color = min(color, 10.0f);
    color = max(color - compute_cbuf.threshold, 0.0f);

    outputTexture[dispatchID.xy] = float4(compute_cbuf.bloom_scale * color, 1.0f);
}


/*
When you use DirectCompute with cs_5_0 profiles, keep the following items in mind:

The maximum number of threads is limited to D3D11_CS_THREAD_GROUP_MAX_THREADS_PER_GROUP (1024) per group.
The X and Y dimension of numthreads is limited to D3D11_CS_THREAD_GROUP_MAX_X (1024) and D3D11_CS_THREAD_GROUP_MAX_Y (1024).
The Z dimension of numthreads is limited to D3D11_CS_THREAD_GROUP_MAX_Z (64).
The maximum dimension of dispatch is limited to D3D11_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION (65535).
The maximum number of unordered-access views that can be bound to a shader is D3D11_PS_CS_UAV_REGISTER_COUNT (8).
Supports RWStructuredBuffers, RWByteAddressBuffers, and typed unordered-access views (RWTexture1D, RWTexture2D, RWTexture3D, and so on).
Atomic instructions are available.
Double-precision support might be available. For information about how to determine whether double-precision is available, see D3D11_FEATURE_DOUBLES.*/