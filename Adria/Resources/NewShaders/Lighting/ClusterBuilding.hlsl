#include "Packing.hlsli"
#include "Common.hlsli"
#include "CommonResources.hlsli"
//reference
//https://github.com/Angelo1211/HybridRenderingEngine/
#define CLUSTER_BUILDING_DISPATCH_SIZE_X 16
#define CLUSTER_BUILDING_DISPATCH_SIZE_Y 16
#define CLUSTER_BUILDING_DISPATCH_SIZE_Z 16

struct ClusterAABB
{
	float4 minPoint;
	float4 maxPoint;
};

struct ClusterBuildingConstants
{
	uint clustersIdx;
};
ConstantBuffer<ClusterBuildingConstants> PassCB : register(b1);

float3 IntersectionZPlane(float3 B, float zDist)
{
	float3 normal = float3(0.0, 0.0, -1.0);
	float3 d = B;
	float t = zDist / d.z;
	float3 result = t * d;
	return result;
}

struct CSInput
{
	uint3 GroupId : SV_GroupID;
	uint3 GroupThreadId : SV_GroupThreadID;
	uint3 DispatchThreadId : SV_DispatchThreadID;
	uint  GroupIndex : SV_GroupIndex;
};

[numthreads(1, 1, 1)]
void ClusterBuilding(CSInput input)
{
	RWStructuredBuffer<ClusterAABB> clusters = ResourceDescriptorHeap[PassCB.clustersIdx];
	uint clustersCount, unused;
	clusters.GetDimensions(clustersCount, unused);

	float2 uv = ((float2) input.DispatchThreadId.xy + 0.5f) * 1.0f / (FrameCB.renderResolution);

	uint3 groupId = input.GroupId;
	uint tileIndex = groupId.x +
		groupId.y * CLUSTER_BUILDING_DISPATCH_SIZE_X +
		groupId.z * (CLUSTER_BUILDING_DISPATCH_SIZE_X * CLUSTER_BUILDING_DISPATCH_SIZE_Y);

	float2 tileSize = rcp(float2(CLUSTER_BUILDING_DISPATCH_SIZE_X, CLUSTER_BUILDING_DISPATCH_SIZE_Y)); 

	float3 maxPoint = GetViewPosition((groupId.xy + 1) * tileSize, 1.0f);
	float3 minPoint = GetViewPosition(groupId.xy * tileSize, 1.0f);

	float clusterNear = FrameCB.cameraNear * pow(FrameCB.cameraFar / FrameCB.cameraNear, groupId.z / float(CLUSTER_BUILDING_DISPATCH_SIZE_Z));
	float clusterFar = FrameCB.cameraNear * pow(FrameCB.cameraFar / FrameCB.cameraNear, (groupId.z + 1) / float(CLUSTER_BUILDING_DISPATCH_SIZE_Z));

	float3 minPointNear = IntersectionZPlane(minPoint, clusterNear);
	float3 minPointFar = IntersectionZPlane(minPoint, clusterFar);
	float3 maxPointNear = IntersectionZPlane(maxPoint, clusterNear);
	float3 maxPointFar = IntersectionZPlane(maxPoint, clusterFar);

	float3 minPointAABB = min(min(minPointNear, minPointFar), min(maxPointNear, maxPointFar));
	float3 maxPointAABB = max(max(minPointNear, minPointFar), max(maxPointNear, maxPointFar));

	clusters[tileIndex].minPoint = float4(minPointAABB, 0.0);
	clusters[tileIndex].maxPoint = float4(maxPointAABB, 0.0);
}