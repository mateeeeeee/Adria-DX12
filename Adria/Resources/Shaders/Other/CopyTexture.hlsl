#include "CommonResources.hlsli"

struct VSToPS
{
	float4 Pos  : SV_POSITION;
	float2 Tex  : TEX;
};

struct CopyTextureConstants
{
	uint inputIdx;
};
ConstantBuffer<CopyTextureConstants> CopyTexturePassCB : register(b1);

float4 CopyTexturePS(VSToPS input) : SV_Target0
{
	Texture2D<float4> inputTexture = ResourceDescriptorHeap[CopyTexturePassCB.inputIdx];
	float4 color = inputTexture.Sample(LinearWrapSampler, input.Tex);
	return color;
}