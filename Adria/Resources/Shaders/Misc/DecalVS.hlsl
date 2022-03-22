#include "../Globals/GlobalsVS.hlsli"

struct VS_INPUT
{
    float3 Pos : POSITION;
};

struct VS_OUTPUT
{
    float4 Position     : SV_POSITION;
    float4 ClipSpacePos : POSITION;
    matrix InverseModel : INVERSE_MODEL;
};

VS_OUTPUT main(VS_INPUT vin)
{
    VS_OUTPUT vout = (VS_OUTPUT) 0;
    float4 world_pos = mul(float4(vin.Pos, 1.0f), object_cbuf.model);
    vout.Position = mul(world_pos, frame_cbuf.viewprojection);
    vout.ClipSpacePos = vout.Position;
    vout.InverseModel = transpose(object_cbuf.inverse_transposed_model);
    return vout;
}