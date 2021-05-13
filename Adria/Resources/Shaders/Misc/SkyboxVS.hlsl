#include "../Globals/GlobalsVS.hlsli"


struct VertexIn
{
    float3 PosL : POSITION;
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float3 PosL : POSITION;
};


VertexOut main(VertexIn vin)
{
    VertexOut vout;

    vout.PosH = mul(float4(vin.PosL, 1.0f), object_cbuf.model);
    vout.PosH = mul(vout.PosH, frame_cbuf.viewprojection).xyww;
    // Use local vertex position as cubemap lookup vector.
    vout.PosL = vin.PosL;

    return vout;
}