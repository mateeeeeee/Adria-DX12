#include "../Globals/GlobalsVS.hlsli"

struct VS_INPUT
{
    float3 Pos : POSITION;
#if TRANSPARENT
    float2 TexCoords : TEX;
#endif
};

struct VS_OUTPUT
{
    float4 Pos : SV_POSITION;
#if TRANSPARENT
    float2 TexCoords : TEX;
#endif
};


VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;
    float4 pos = float4(input.Pos, 1.0f);
    pos = mul(pos, object_cbuf.model);
    pos = mul(pos, shadow_cbuf.lightviewprojection);
    output.Pos = pos;
    
#if TRANSPARENT
    output.TexCoords = input.TexCoords;
#endif
    
    return output;
}