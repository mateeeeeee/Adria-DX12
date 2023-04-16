#include "LightRadiance.hlsli"
#include "../Packing.hlsli"

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
	uint lightIndexCounterIdx; //1
	uint lightIndexListIdx;	   //MAX_CLUSTER_LIGHTS * 16^3
	uint lightGridIdx;		   //16^3
};
ConstantBuffer<ClusterCullingConstants> PassCB : register(b1);

bool LightIntersectsCluster(Light light, ClusterAABB cluster)
{
	if (light.type == DIRECTIONAL_LIGHT)return true;
	float3 closest = max(cluster.minPoint, min(light.position, cluster.maxPoint)).xyz;
	float3 dist = closest - light.position.xyz;
	return dot(dist, dist) <= (light.range * light.range);
}

struct CS_INPUT
{
	uint3 GroupId : SV_GroupID;
	uint3 GroupThreadId : SV_GroupThreadID;
	uint3 DispatchThreadId : SV_DispatchThreadID;
	uint  GroupIndex : SV_GroupIndex;
};

groupshared Light shared_lights[GROUP_SIZE];

[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void ClusterCulling(CS_INPUT input)
{
	StructuredBuffer<ClusterAABB> clusters			 = ResourceDescriptorHeap[PassCB.clustersIdx];
	StructuredBuffer<Light> lights					 = ResourceDescriptorHeap[FrameCB.lightsIdx];
	RWStructuredBuffer<uint> lightIndexList			 = ResourceDescriptorHeap[PassCB.lightIndexListIdx];
	RWStructuredBuffer<uint> lightIndexCounter		 = ResourceDescriptorHeap[PassCB.lightIndexCounterIdx];
	RWStructuredBuffer<LightGrid> lightGrid          = ResourceDescriptorHeap[PassCB.lightGridIdx];

	if (all(input.DispatchThreadId == 0))
	{
		lightIndexCounter[0] = 0;
	}

	uint visibleLightCount = 0;
	uint visibleLightIndices[MAX_CLUSTER_LIGHTS];

	uint clusterIndex = input.GroupIndex + GROUP_SIZE * input.GroupId.z;
	ClusterAABB cluster = clusters[clusterIndex];

	uint lightOffset = 0;
	uint lightCount, _unused;
	lights.GetDimensions(lightCount, _unused);

	while (lightOffset < lightCount)
	{
		uint batchSize = min(GROUP_SIZE, lightCount - lightOffset);
		if (input.GroupIndex < batchSize)
		{
			uint lightIndex = lightOffset + input.GroupIndex;
			Light light = lights[lightIndex];
			shared_lights[input.GroupIndex] = light;
		}
		GroupMemoryBarrierWithGroupSync();
		for (uint i = 0; i < batchSize; i++)
		{
			Light light = lights[i];
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
	lightGrid[clusterIndex].offset = offset;
	lightGrid[clusterIndex].lightCount = visibleLightCount;
}