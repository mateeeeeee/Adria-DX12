
#include "../Globals/GlobalsPS.hlsli"
#include "../Util/RootSignatures.hlsli"


Texture2D txDiffuse : register(t0);

SamplerState linear_wrap_sampler : register(s0);


struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEX;
};

[RootSignature(Forward_RS)]
float4 main(PS_INPUT IN) : SV_TARGET
{


    float4 texColor = txDiffuse.Sample(linear_wrap_sampler, IN.TexCoord) * float4(material_cbuf.diffuse, 1.0);
    
    return texColor;
}
