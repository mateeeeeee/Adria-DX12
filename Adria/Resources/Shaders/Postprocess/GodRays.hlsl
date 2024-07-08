#include "CommonResources.hlsli"

#define BLOCK_SIZE 16
#define NUM_SAMPLES 64

struct GodRaysConstants
{
	float2 sunScreenSpacePosition;
	float  density;
	float  weight;
	float  decay;
	float  exposure;

	uint   sunIdx;
	uint   outputIdx;
};
ConstantBuffer<GodRaysConstants> GodRaysPassCB : register(b1);

struct CSInput
{
	uint3 GroupId : SV_GroupID;
	uint3 GroupThreadId : SV_GroupThreadID;
	uint3 DispatchThreadId : SV_DispatchThreadID;
	uint GroupIndex : SV_GroupIndex;
};

[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void GodRaysCS(CSInput input)
{
	Texture2D<float4> sunTexture = ResourceDescriptorHeap[GodRaysPassCB.sunIdx];
	RWTexture2D<float4> outputTexture = ResourceDescriptorHeap[GodRaysPassCB.outputIdx];
	float2 uv = ((float2) input.DispatchThreadId.xy + 0.5f) * 1.0f / (FrameCB.renderResolution);

	float3 color = sunTexture.SampleLevel(LinearClampSampler, uv, 0).rgb;

	float2 deltaUV = (uv - GodRaysPassCB.sunScreenSpacePosition);
	deltaUV *= GodRaysPassCB.density / NUM_SAMPLES;

	float illuminationDecay = 1.0f;
	float3 accumulatedGodRays = 0.0f;
	float3 accumulated = 0.0f;
	for (int i = 0; i < NUM_SAMPLES; i++)
	{
		uv -= deltaUV;
		float3 sam = sunTexture.SampleLevel(LinearClampSampler, uv, 0).rgb;
		sam *= illuminationDecay * GodRaysPassCB.weight;
		accumulated += sam;
		illuminationDecay *= GodRaysPassCB.decay;
	}
	accumulated *= GodRaysPassCB.exposure;
	outputTexture[input.DispatchThreadId.xy] = float4(color + accumulated, 1.0f);
}