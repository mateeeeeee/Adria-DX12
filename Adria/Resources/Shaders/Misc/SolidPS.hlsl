#include "../Globals/GlobalsPS.hlsli"
#include "../Util/RootSignatures.hlsli"

struct PS_INPUT
{
    float4 Pos : SV_POSITION;
};



[RootSignature(Forward_RS)]
float4 main(PS_INPUT pin) : SV_Target
{
    return float4(material_cbuf.diffuse, 1.0);
}