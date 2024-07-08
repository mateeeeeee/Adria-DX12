#include "CommonResources.hlsli"
#include "Random.hlsli"
#include "RainUtil.hlsli"

#define BLOCK_SIZE 256

struct RainSimulationConstants
{
	uint   rainDataIdx;
	uint   depthIdx;
	float  simulationSpeed;
	float  rangeRadius;
};
ConstantBuffer<RainSimulationConstants> RainSimulationPassCB : register(b1);

struct RainData
{
	float3 Pos;
	float3 Vel;
	float  State;
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
	
	RWStructuredBuffer<RainData> rainDataBuffer = ResourceDescriptorHeap[RainSimulationPassCB.rainDataIdx];
	uint GroupIdx = dispatchThreadId.x;

	RainData rainDrop = rainDataBuffer[GroupIdx];
	rainDrop.Pos += rainDrop.Vel * FrameCB.deltaTime * RainSimulationPassCB.simulationSpeed; 
	
	const float3 boundsCenter = FrameCB.cameraPosition.xyz;
	const float3 boundsExtents = float3(RainSimulationPassCB.rangeRadius, RainSimulationPassCB.rangeRadius, RainSimulationPassCB.rangeRadius); 
	
	float2 offsetAmount = (rainDrop.Pos.xz - boundsCenter.xz) / boundsExtents.xz;
	rainDrop.Pos.xz -= boundsExtents.xz * ceil(0.5 * offsetAmount - 0.5);

	bool OutsideBounds = abs(rainDrop.Pos.y - boundsCenter.y) > boundsExtents.y;
	if(OutsideBounds)
	{
		uint randSeed = InitRand(GroupIdx, 47, 16);
		float4 random01 = float4(NextRand(randSeed), NextRand(randSeed), NextRand(randSeed), NextRand(randSeed));
		float4 random_11 = (random01 * 2.0) - 1.0;
	
		rainDrop.Pos.xyz = boundsCenter.xyz + boundsExtents.xyz * random_11.xyz;
		rainDrop.Pos.y -= dot(random01.zw, 0.2f) * boundsExtents.y;

		//float3 windDir = FrameCB.windParams.xyz * FrameCB.windParams.w;
		//rainDrop.Vel.xz += windDir.xz * random01.zw;
	}

	bool Blocked = IsBlockedFromRain(rainDrop.Pos.xyz);
	rainDrop.State = Blocked ? -1.0f : 1.0f;
	rainDataBuffer[GroupIdx] = rainDrop;
}