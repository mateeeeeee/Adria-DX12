#include "CommonResources.hlsli"

#define BLOCK_SIZE 16

struct MotionVectorsConstants
{
	uint depthIdx;
	uint outputIdx;
};

ConstantBuffer<MotionVectorsConstants> MotionVectorsPassCB : register(b1);

struct CSInput
{
	uint3 GroupId : SV_GroupID;
	uint3 GroupThreadId : SV_GroupThreadID;
	uint3 DispatchThreadId : SV_DispatchThreadID;
	uint  GroupIndex : SV_GroupIndex;
};

[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void MotionVectorsCS(CSInput input)
{
	Texture2D<float> depthTexture = ResourceDescriptorHeap[MotionVectorsPassCB.depthIdx];
	RWTexture2D<float2> velocityTexture = ResourceDescriptorHeap[MotionVectorsPassCB.outputIdx];

	float2 uv = ((float2)input.DispatchThreadId.xy + 0.5f) * 1.0f / (FrameCB.renderResolution);
	float2 currentClip = uv * float2(2, -2) + float2(-1, 1);
	float depth = depthTexture[input.DispatchThreadId.xy];
	float4 previousClip = mul(float4(currentClip, depth, 1.0f), FrameCB.reprojection);
	previousClip.xy /= previousClip.w;
	velocityTexture[input.DispatchThreadId.xy] = (previousClip.xy - currentClip) * float2(0.5f, -0.5f);
}