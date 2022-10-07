#include "../Globals/GlobalsCS.hlsli"

Texture2D<float4> inputTexture : register(t0);
Texture2D<float4> bloomTexture : register(t1);

RWTexture2D<float4> outputTexture : register(u0);

SamplerState linear_clamp_sampler : register(s0);


[numthreads(32, 32, 1)]
void main(uint3 groupID : SV_GroupID, uint3 groupThreadID : SV_GroupThreadID, uint groupIndex : SV_GroupIndex, uint3 dispatchID : SV_DispatchThreadID)
{
    uint3 dims;
    bloomTexture.GetDimensions(0, dims.x, dims.y, dims.z);

    float3 bloom = bloomTexture.SampleLevel(linear_clamp_sampler, dispatchID.xy * 1.0f / dims.xy, 1.5f).xyz;
    bloom += bloomTexture.SampleLevel(linear_clamp_sampler, dispatchID.xy * 1.0f / dims.xy, 2.5f).xyz;
    bloom += bloomTexture.SampleLevel(linear_clamp_sampler, dispatchID.xy * 1.0f / dims.xy, 3.5f).xyz;
    bloom /= 3;

    outputTexture[dispatchID.xy] = inputTexture[dispatchID.xy] + float4(bloom, 0.0f);
}