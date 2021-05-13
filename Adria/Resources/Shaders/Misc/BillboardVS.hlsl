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

    float4x4 model_matrix = object_cbuf.model;
    float4x4 model_view = mul(model_matrix, frame_cbuf.view);

    model_view[0][0] = 1;
    model_view[0][1] = 0;
    model_view[0][2] = 0;
    model_view[1][0] = 0;
    model_view[1][1] = 1;
    model_view[1][2] = 0;
    model_view[2][0] = 0;
    model_view[2][1] = 0;
    model_view[2][2] = 1;

    float4 ViewPosition = mul(float4(vin.Pos, 1.0), model_view);

    vout.Position = mul(ViewPosition, frame_cbuf.projection);
    vout.TexCoord = vin.Uvs;
    return vout;
}