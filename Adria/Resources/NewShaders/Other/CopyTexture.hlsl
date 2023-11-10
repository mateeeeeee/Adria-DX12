#include "../CommonResources.hlsli"

struct VSToPS
{
	float4 Pos  : SV_POSITION;
	float2 Tex  : TEX;
};

struct CopyTextureConstants
{
	uint inputIdx;
};
ConstantBuffer<CopyTextureConstants> PassCB : register(b1);

float4 CopyTexture(VSToPS input) : SV_Target0
{
	Texture2D<float4> inputTx = ResourceDescriptorHeap[PassCB.inputIdx];
	float4 color = inputTx.Sample(LinearWrapSampler, input.Tex);
	return color;
}