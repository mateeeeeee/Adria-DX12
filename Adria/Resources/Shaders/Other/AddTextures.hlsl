#include "CommonResources.hlsli"

struct VSToPS
{
	float4 Pos : SV_POSITION;
	float2 Tex  : TEX;
};

struct AddTexturesConstants
{
	uint inputIdx1;
	uint inputIdx2;
};
ConstantBuffer<AddTexturesConstants> AddTexturesPassCB : register(b1);

float4 AddTexturesPS(VSToPS input) : SV_Target0
{
	Texture2D<float4> inputTexture1 = ResourceDescriptorHeap[AddTexturesPassCB.inputIdx1];
	Texture2D<float4> inputTexture2 = ResourceDescriptorHeap[AddTexturesPassCB.inputIdx2];
	float4 color1 = inputTexture1.Sample(LinearWrapSampler, input.Tex);
	float4 color2 = inputTexture2.Sample(LinearWrapSampler, input.Tex);
	return color1 + color2;
}