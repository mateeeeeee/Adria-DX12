#include "CommonResources.hlsli"

#define BLOCK_SIZE 16
#define SAMPLE_COUNT 8

struct MotionBlurConstants
{
	uint  sceneIdx;
	uint  velocityIdx;
	uint  outputIdx;
};
ConstantBuffer<MotionBlurConstants> MotionBlurPassCB : register(b1);

struct CSInput
{
	uint3 GroupId : SV_GroupID;
	uint3 GroupThreadId : SV_GroupThreadID;
	uint3 DispatchThreadId : SV_DispatchThreadID;
	uint GroupIndex : SV_GroupIndex;
};

[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void MotionBlurCS(CSInput input)
{
	Texture2D<float4> sceneTexture = ResourceDescriptorHeap[MotionBlurPassCB.sceneIdx];
	Texture2D<float2> velocityTexture = ResourceDescriptorHeap[MotionBlurPassCB.velocityIdx];
	RWTexture2D<float4> outputTexture = ResourceDescriptorHeap[MotionBlurPassCB.outputIdx];

	float2 uv = ((float2) input.DispatchThreadId.xy + 0.5f) * 1.0f / (FrameCB.displayResolution);

	float2 velocity = velocityTexture.Sample(LinearWrapSampler, uv) / SAMPLE_COUNT;
	float4 color = sceneTexture.Sample(LinearWrapSampler, uv);
	uv += velocity;
	for (int i = 1; i < SAMPLE_COUNT; ++i, uv += velocity)
	{
		float4 currentColor = sceneTexture.Sample(LinearClampSampler, uv);
		color += currentColor;
	}
	float4 finalColor = color / SAMPLE_COUNT;
	outputTexture[input.DispatchThreadId.xy] = finalColor;
}