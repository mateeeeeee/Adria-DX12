#include "../Util/RootSignatures.hlsli"
#include "../Globals/GlobalsPS.hlsli"

Texture2D sceneTx : register(t0);
Texture2D<float> depthTx : register(t1);


SamplerState linear_wrap_sampler : register(s0);


struct VertexOut
{
    float4 PosH : SV_POSITION;
    float2 Tex : TEX;
};


[RootSignature(Fog_RS)]
float4 main(VertexOut pin) : SV_TARGET
{
    float4 main_color = sceneTx.Sample(linear_wrap_sampler, pin.Tex);
               
    float depth = depthTx.Sample(linear_wrap_sampler, pin.Tex);
    float3 pos_vs = GetPositionVS(pin.Tex, depth);

    float fog = 0.0f;
    if (postprocess_cbuf.fog_type == EXPONENTIAL_FOG)
    {
        fog = ExponentialFog(float4(pos_vs, 1.0f));
    }
    else if (postprocess_cbuf.fog_type == EXPONENTIAL_HEIGHT_FOG)
    {
        fog = ExponentialHeightFog(float4(pos_vs, 1.0f));
    }
    return lerp(main_color, postprocess_cbuf.fog_color, fog);
}