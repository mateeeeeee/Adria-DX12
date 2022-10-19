#include "../CommonResources.hlsli"

#define BLOCK_SIZE 16

struct MotionVectorsConstants
{
	uint depthIdx;
	uint outputIdx;
};

ConstantBuffer<MotionVectorsConstants> PassCB : register(b1);

struct CS_INPUT
{
	uint3 GroupId : SV_GroupID;
	uint3 GroupThreadId : SV_GroupThreadID;
	uint3 DispatchThreadId : SV_DispatchThreadID;
	uint  GroupIndex : SV_GroupIndex;
};

[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void MotionVectors(CS_INPUT input)
{
	Texture2D<float> depthTx = ResourceDescriptorHeap[PassCB.depthIdx];
	RWTexture2D<float2> velocityTx = ResourceDescriptorHeap[PassCB.outputIdx];

	float2 uv = ((float2)input.DispatchThreadId.xy + 0.5f) * 1.0f / (FrameCB.screenResolution);
	float depth = depthTx[input.DispatchThreadId.xy];
	float4 pos = float4(uv, depth, 1);

	float4 posCopy = pos; 
	posCopy.xy *= 2.0f; posCopy.xy -= 1.0f; posCopy.y *= -1.0f;
	float4 prevPos = mul(posCopy, FrameCB.reprojection);
	prevPos.xyz /= prevPos.w;
	prevPos.xy *= float2(0.5f, -0.5f);
	prevPos.xy += 0.5f;
	velocityTx[input.DispatchThreadId.xy] = (prevPos - pos).xy;
}