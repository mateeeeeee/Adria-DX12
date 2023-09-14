#include "../CommonResources.hlsli"
#include "../Packing.hlsli"

struct VS_OUTPUT
{
	float4 Position : SV_Position;
	float4 Color : COLOR;
};

struct VS_INPUT
{
	float3 Position : POSITION;
	uint Color : COLOR;
};

VS_OUTPUT DebugVS(VS_INPUT v)
{
	VS_OUTPUT output = (VS_OUTPUT)0;
	output.Position = mul(float4(v.Position, 1.0f), FrameCB.viewProjection);
	output.Color = UnpackUintColor(v.Color);
	return output;
}

float4 DebugPS(VS_OUTPUT input) : SV_Target
{
	return input.Color; 
}