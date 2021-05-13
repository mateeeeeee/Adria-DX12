#include "../Globals/GlobalsVS.hlsli"


struct VS_INPUT
{
    float3 Pos : POSITION;
    float2 Uvs : TEX;
};

struct VS_OUTPUT
{
    
    float4 Position : SV_POSITION;
    float2 TexCoord : TEX;
};



VS_OUTPUT main(VS_INPUT vin)
{
    VS_OUTPUT vout;
    
    vout.Position = mul(mul(float4(vin.Pos, 1.0), object_cbuf.model), frame_cbuf.viewprojection);
    vout.TexCoord = vin.Uvs;
    return vout;
}