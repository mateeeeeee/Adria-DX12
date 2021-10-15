#include "../Globals/GlobalsPS.hlsli"
#include "../Util/RootSignatures.hlsli"

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float3 PosL : POSITION;
};

[RootSignature(Sky_RS)]
float4 main(VertexOut pin) : SV_Target
{
    return weather_cbuf.sky_color;
}