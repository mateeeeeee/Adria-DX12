#include "../Globals/GlobalsVS.hlsli"


struct VS_INPUT
{
    float3 Position : POSITION;
    float2 Uvs : TEX;
    float3 Normal : NORMAL;
    float3 Tan : TANGENT;
    float3 Bitan : BITANGENT;

};



struct VS_OUTPUT
{
    float4 Position : SV_POSITION;

    float2 Uvs : TEX;
    float3 NormalVS : NORMAL0;

    float3 TangentWS : TANGENT;
    float3 BitangentWS : BITANGENT;
    float3 NormalWS : NORMAL1;
    matrix view_matrix : MATRIX0;

};


VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT Output;
    
    float4 pos = mul(float4(input.Position, 1.0), object_cbuf.model);
    Output.Position = mul(pos, frame_cbuf.viewprojection);

    Output.Uvs = input.Uvs;

	// Transform the normal to world space
    float3 normal_ws = mul(input.Normal, (float3x3) object_cbuf.inverse_transposed_model);
    Output.NormalVS = mul(normal_ws, (float3x3) transpose(frame_cbuf.inverse_view));

    Output.TangentWS = mul(input.Tan, (float3x3) object_cbuf.model);
    Output.BitangentWS = mul(input.Bitan, (float3x3) object_cbuf.model);
    Output.NormalWS = normal_ws;
    Output.view_matrix = frame_cbuf.view;

    return Output;
}