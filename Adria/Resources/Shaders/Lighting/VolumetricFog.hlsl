#include "CommonResources.hlsli"

#define BLOCK_SIZE 16

struct CSInput
{
	uint3 GroupId : SV_GroupID;
	uint3 GroupThreadId : SV_GroupThreadID;
	uint3 DispatchThreadId : SV_DispatchThreadID;
	uint  GroupIndex : SV_GroupIndex;
};

struct FogVolume
{
	float3  center;
	float3  extents;
	float3  color;
	float   density_base;
	float   density_change;
};

struct LightInjectionConstants
{
	uint voxelGridIdx;
	uint voxelGridHistoryIdx;
	uint fogVolumeBufferIdx;
};
ConstantBuffer<LightInjectionConstants> PassCB : register(b1);


[numthreads(8, 8, 8)]
void LightInjectionCS(CSInput input)
{
	uint3 voxelGridCoords = input.DispatchThreadId;
}

struct ScatteringAccumulationConstants
{
	uint voxelGridIdx;
	uint voxelGridHistoryIdx;
	uint fogVolumeBufferIdx;
};
ConstantBuffer<ScatteringAccumulationConstants> PassCB2 : register(b1);

[numthreads(8, 8, 8)]
void ScatteringAccumulationCS(CSInput input)
{
	
}




