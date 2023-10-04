#include "../CommonResources.hlsli"

#define BLOCK_SIZE 16
#define SAMPLE_COUNT 8

struct MotionBlurConstants
{
	uint  sceneIdx;
	uint  velocityIdx;
	uint  outputIdx;
};
ConstantBuffer<MotionBlurConstants> PassCB : register(b1);

struct CSInput
{
	uint3 GroupId : SV_GroupID;
	uint3 GroupThreadId : SV_GroupThreadID;
	uint3 DispatchThreadId : SV_DispatchThreadID;
	uint GroupIndex : SV_GroupIndex;
};

[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void MotionBlur(CSInput input)
{
	Texture2D<float4> sceneTx = ResourceDescriptorHeap[PassCB.sceneIdx];
	Texture2D<float2> velocityTx = ResourceDescriptorHeap[PassCB.velocityIdx];
	RWTexture2D<float4> outputTx = ResourceDescriptorHeap[PassCB.outputIdx];

	float2 uv = ((float2) input.DispatchThreadId.xy + 0.5f) * 1.0f / (FrameCB.screenResolution);

	float2 velocity = velocityTx.Sample(LinearWrapSampler, uv) / SAMPLE_COUNT;
	float4 color = sceneTx.Sample(LinearWrapSampler, uv);
	uv += velocity;
	for (int i = 1; i < SAMPLE_COUNT; ++i, uv += velocity)
	{
		float4 currentColor = sceneTx.Sample(LinearClampSampler, uv);
		color += currentColor;
	}
	float4 finalColor = color / SAMPLE_COUNT;
	outputTx[input.DispatchThreadId.xy] = finalColor;
}