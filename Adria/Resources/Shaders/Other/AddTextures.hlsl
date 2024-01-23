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
ConstantBuffer<AddTexturesConstants> PassCB : register(b1);

float4 AddTextures(VSToPS input) : SV_Target0
{
	Texture2D<float4> inputTx1 = ResourceDescriptorHeap[PassCB.inputIdx1];
	Texture2D<float4> inputTx2 = ResourceDescriptorHeap[PassCB.inputIdx2];
	float4 color1 = inputTx1.Sample(LinearWrapSampler, input.Tex);
	float4 color2 = inputTx2.Sample(LinearWrapSampler, input.Tex);
	return color1 + color2;
}