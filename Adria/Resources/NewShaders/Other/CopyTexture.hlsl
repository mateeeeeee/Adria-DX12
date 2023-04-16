#include "../CommonResources.hlsli"

struct VertexOut
{
	float4 PosH : SV_POSITION;
	float2 Tex  : TEX;
};

struct CopyTextureConstants
{
	uint inputIdx;
};
ConstantBuffer<CopyTextureConstants> PassCB : register(b1);

float4 CopyTexture(VertexOut pin) : SV_Target0
{
	Texture2D<float4> inputTx = ResourceDescriptorHeap[PassCB.inputIdx];
	float4 color = inputTx.Sample(LinearWrapSampler, pin.Tex);
	return color;
}