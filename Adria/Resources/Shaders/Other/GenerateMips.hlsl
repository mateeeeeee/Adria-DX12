#include "CommonResources.hlsli"

Texture2D<float4> SrcTexture : register(t0);
RWTexture2D<float4> DstTexture : register(u0);

cbuffer PassCB : register(b1)
{
	float2 texelSize;
	uint   srcIdx;
	uint   dstIdx;
}

[numthreads(8, 8, 1)]
void GenerateMips(uint3 DTid : SV_DispatchThreadID)
{
	Texture2D<float4> SrcTexture = ResourceDescriptorHeap[srcIdx];
	RWTexture2D<float4> DstTexture = ResourceDescriptorHeap[dstIdx];

	float2 uv = (DTid.xy + 0.5) * texelSize;
	DstTexture[DTid.xy] = SrcTexture.SampleLevel(LinearClampSampler, uv, 0);
}

