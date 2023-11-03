#include "../CommonResources.hlsli"

#define BLOCK_SIZE 16

struct MotionVectorsConstants
{
	uint depthIdx;
	uint outputIdx;
};

ConstantBuffer<MotionVectorsConstants> PassCB : register(b1);

struct CSInput
{
	uint3 GroupId : SV_GroupID;
	uint3 GroupThreadId : SV_GroupThreadID;
	uint3 DispatchThreadId : SV_DispatchThreadID;
	uint  GroupIndex : SV_GroupIndex;
};

[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void MotionVectors(CSInput input)
{
	Texture2D<float> depthTx = ResourceDescriptorHeap[PassCB.depthIdx];
	RWTexture2D<float2> velocityTx = ResourceDescriptorHeap[PassCB.outputIdx];

	float2 uv = ((float2)input.DispatchThreadId.xy + 0.5f) * 1.0f / (FrameCB.renderResolution);
	float2 currentClip = uv * float2(2, -2) + float2(-1, 1);
	float depth = depthTx[input.DispatchThreadId.xy];
	float4 previousClip = mul(float4(currentClip, depth, 1.0f), FrameCB.reprojection);
	previousClip.xy /= previousClip.w;
	velocityTx[input.DispatchThreadId.xy] = (previousClip.xy - currentClip) * float2(0.5f, -0.5f);
}