#include "CommonResources.hlsli"

#define BLOCK_SIZE 16*16

struct Constants
{
	uint   depthIdx;
	uint   noiseIdx;
	uint   rainDataIdx;
};
ConstantBuffer<Constants> PassCB : register(b1);

struct RainData
{
	float3 Pos;
	float3 Vel;
	float State;
};

struct CSInput
{
	uint3 GroupId : SV_GroupID;
	uint3 GroupThreadId : SV_GroupThreadID;
	uint3 DispatchThreadId : SV_DispatchThreadID;
	uint  GroupIndex : SV_GroupIndex;
};

[numthreads(BLOCK_SIZE, 1, 1)]
void RainSimulationCS(CSInput input)
{
	uint3 dispatchThreadId = input.DispatchThreadId;
	
	RWStructuredBuffer<RainData> rainDataBuffer = ResourceDescriptorHeap[PassCB.rainDataIdx];
	uint GroupIdx = dispatchThreadId.x: 
	RainData rainDrop = rainDataBuffer[GroupIdx];
	rainDrop.Pos += rainDrop.Vel * FrameCB.deltaTime;
}