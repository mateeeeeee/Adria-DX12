#include "../CommonResources.hlsli"

struct VertexOut
{
	float4 PosH : SV_POSITION;
	float2 Tex  : TEX;
};

struct AddTexturesConstants
{
	uint inputIdx1;
	uint inputIdx2;
};
ConstantBuffer<AddTexturesConstants> PassCB : register(b1);

float4 AddTextures(VertexOut pin) : SV_Target0
{
	Texture2D<float4> inputTx1 = ResourceDescriptorHeap[PassCB.inputIdx1];
	Texture2D<float4> inputTx2 = ResourceDescriptorHeap[PassCB.inputIdx2];
	float4 color1 = inputTx1.Sample(LinearWrapSampler, pin.Tex);
	float4 color2 = inputTx2.Sample(LinearWrapSampler, pin.Tex);
	return color1 + color2;
}