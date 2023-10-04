#include "../CommonResources.hlsli"
#include "../Packing.hlsli"

struct VSToPS
{
	float4 Position : SV_Position;
	float4 Color : COLOR;
};

struct VSInput
{
	float3 Position : POSITION;
	uint Color : COLOR;
};

VSToPS DebugVS(VSInput input)
{
	VSToPS output = (VSToPS)0;
	output.Position = mul(float4(input.Position, 1.0f), FrameCB.viewProjection);
	output.Color = UnpackUintColor(input.Color);
	return output;
}

float4 DebugPS(VSToPS input) : SV_Target
{
	return input.Color; 
}