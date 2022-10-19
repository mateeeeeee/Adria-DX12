#include "../Util/RootSignatures.hlsli"
SamplerState linear_wrap_sampler : register(s0);
Texture2D<float4> Texture : register(t0);


struct VertexOut
{
    float4 PosH : SV_POSITION;
    float2 Tex : TEX;
};

[RootSignature(Fxaa_RS)]
float4 main(VertexOut pin) : SV_Target0
{
    float4 color = Texture.Sample(linear_wrap_sampler, pin.Tex);
    return color;
}