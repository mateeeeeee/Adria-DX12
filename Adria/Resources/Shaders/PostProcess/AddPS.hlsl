#include "../Util/RootSignatures.hlsli"

SamplerState linear_wrap_sampler : register(s0);
Texture2D<float4> Texture1 : register(t0);
Texture2D<float4> Texture2 : register(t1);

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float2 Tex : TEX;
};


[RootSignature(Add_RS)]
float4 main(VertexOut pin) : SV_Target0
{
    float4 color1 = Texture1.Sample(linear_wrap_sampler, pin.Tex);
    float4 color2 = Texture2.Sample(linear_wrap_sampler, pin.Tex);
    return color1 + color2;
}