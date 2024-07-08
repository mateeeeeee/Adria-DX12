#include "Lighting.hlsli"
#include "Packing.hlsli"

//references 
//https://github.com/pezcode/Cluster
#define BLOCK_SIZE 16
#define GROUP_SIZE (16*16*1)
#define MAX_CLUSTER_LIGHTS 128
#define CLUSTER_CULLING_DISPATCH_SIZE_X 1
#define CLUSTER_CULLING_DISPATCH_SIZE_Y 1
#define CLUSTER_CULLING_DISPATCH_SIZE_Z 16

struct ClusterAABB
{
	float4 minPoint;
	float4 maxPoint;
};

struct LightGrid
{
	uint offset;
	uint lightCount;
};

struct ClusterCullingConstants
{
	uint clustersIdx;
	uint lightIndexCounterIdx; 
	uint lightIndexListIdx;	   
	uint lightGridIdx;		   
};
ConstantBuffer<ClusterCullingConstants> ClusterCullingPassCB : register(b1);

bool LightIntersectsCluster(Light light, ClusterAABB cluster)
{
	if (light.type == DIRECTIONAL_LIGHT) return true;
	float3 closest = max(cluster.minPoint, min(light.position, cluster.maxPoint)).xyz;
	float3 dist = closest - light.position.xyz;
	return dot(dist, dist) <= (light.range * light.range);
}

struct CSInput
{
	uint3 GroupId : SV_GroupID;
	uint3 GroupThreadId : SV_GroupThreadID;
	uint3 DispatchThreadId : SV_DispatchThreadID;
	uint  GroupIndex : SV_GroupIndex;
};

groupshared Light SharedLights[GROUP_SIZE];

[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void ClusterCullingCS(CSInput input)
{
	StructuredBuffer<ClusterAABB> clusterBuffer		 = ResourceDescriptorHeap[ClusterCullingPassCB.clustersIdx];
	StructuredBuffer<Light> lightBuffer				 = ResourceDescriptorHeap[FrameCB.lightsIdx];
	RWStructuredBuffer<uint> lightIndexList			 = ResourceDescriptorHeap[ClusterCullingPassCB.lightIndexListIdx];
	RWStructuredBuffer<uint> lightIndexCounter		 = ResourceDescriptorHeap[ClusterCullingPassCB.lightIndexCounterIdx];
	RWStructuredBuffer<LightGrid> lightGridBuffer    = ResourceDescriptorHeap[ClusterCullingPassCB.lightGridIdx];

	if (all(input.DispatchThreadId == 0))
	{
		lightIndexCounter[0] = 0;
	}

	uint visibleLightCount = 0;
	uint visibleLightIndices[MAX_CLUSTER_LIGHTS];

	uint clusterIndex = input.GroupIndex + GROUP_SIZE * input.GroupId.z;
	ClusterAABB cluster = clusterBuffer[clusterIndex];

	uint lightOffset = 0;
	uint lightCount, _unused;
	lightBuffer.GetDimensions(lightCount, _unused);

	while (lightOffset < lightCount)
	{
		uint batchSize = min(GROUP_SIZE, lightCount - lightOffset);
		if (input.GroupIndex < batchSize)
		{
			uint lightIndex = lightOffset + input.GroupIndex;
			Light light = lightBuffer[lightIndex];
			SharedLights[input.GroupIndex] = light;
		}
		GroupMemoryBarrierWithGroupSync();
		for (uint i = 0; i < batchSize; i++)
		{
			Light light = lightBuffer[i];
			if (!light.active) continue;
			if (visibleLightCount < MAX_CLUSTER_LIGHTS && LightIntersectsCluster(light, cluster))
			{
				visibleLightIndices[visibleLightCount] = lightOffset + i;
				visibleLightCount++;
			}
		}
		lightOffset += batchSize;
	}
	GroupMemoryBarrierWithGroupSync();
	uint offset = 0;
	InterlockedAdd(lightIndexCounter[0], visibleLightCount, offset);
	for (uint i = 0; i < visibleLightCount; i++)
	{
		lightIndexList[offset + i] = visibleLightIndices[i];
	}
	lightGridBuffer[clusterIndex].offset = offset;
	lightGridBuffer[clusterIndex].lightCount = visibleLightCount;
}