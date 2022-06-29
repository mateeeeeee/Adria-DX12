#include "../Globals/GlobalsPS.hlsli"
#include "../Util/RootSignatures.hlsli"



struct VertexOut
{
    float4 PosH : SV_POSITION;
    float3 PosL : POSITION;
};


SamplerState linear_sampler : register(s0);
TextureCube cubemap : register(t0);


[RootSignature(Skybox_RS)]
float4 main(VertexOut pin) : SV_Target
{
    return cubemap.Sample(linear_sampler, pin.PosL);
}