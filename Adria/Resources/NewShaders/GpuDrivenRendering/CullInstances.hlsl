#include "GpuDrivenRendering.hlsli"
#include "../CommonResources.hlsli"
#include "../Common.hlsli"
#include "../Scene.hlsli"

#define BLOCK_SIZE 64

struct CullInstancesConstants
{
	uint numInstances;
	uint hzbIdx;
	uint occludedInstancesIdx;
	uint occludedInstancesCounterIdx;
	uint candidateMeshletsIdx;
	uint candidateMeshletsCounterIdx;
};
ConstantBuffer<CullInstancesConstants> PassCB : register(b1);

[numthreads(BLOCK_SIZE, 1, 1)]
void CullInstancesCS(uint threadId : SV_DispatchThreadID)
{
#if !SECOND_PHASE
	uint numInstances = PassCB.numInstances;
#else
	Buffer<uint> occludedInstancesCounter = ResourceDescriptorHeap[PassCB.occludedInstancesCounterIdx];
	uint numInstances = occludedInstancesCounter[0];
#endif
	if (threadId >= numInstances) return;

	Instance instance = GetInstanceData(threadId);
	Mesh mesh = GetMeshData(instance.meshIndex);

	FrustumCullData cullData = FrustumCull(instance.bbOrigin, instance.bbExtents, instance.worldMatrix, FrameCB.viewProjection);
	bool isVisible = cullData.isVisible;
	bool wasOccluded = false;

#if 0
	if (isVisible)
	{
		Texture2D<float> hzbTx = ResourceDescriptorHeap[PassCB.hzbIdx];
#if !SECOND_PHASE
		FrustumCullData prevCullData = FrustumCull(instance.bbOrigin, instance.bbExtents, instance.worldMatrix, FrameCB.prevViewProjection);
		wasOccluded = !HZBCull(prevCullData, hzbTx);
		if (wasOccluded)
		{
			RWBuffer<uint> occludedInstancesCounter = ResourceDescriptorHeap[PassCB.occludedInstancesCounterIdx];
			RWStructuredBuffer<uint> occludedInstances = ResourceDescriptorHeap[PassCB.occludedInstancesIdx];
			uint elementOffset = 0;
			InterlockedAdd(occludedInstancesCounter[0], 1, elementOffset);
			occludedInstances[elementOffset] = instance.instanceId;
		}
#else
		isVisible = HZBCull(cullData, hzbTx);
#endif
	}
#endif

	if (isVisible && !wasOccluded)
	{
		RWBuffer<uint> candidateMeshletsCounter = ResourceDescriptorHeap[PassCB.candidateMeshletsCounterIdx];
		RWStructuredBuffer<MeshletCandidate> candidateMeshlets = ResourceDescriptorHeap[PassCB.candidateMeshletsIdx];

		uint globalMeshletIndex;
		InterlockedAdd(candidateMeshletsCounter[COUNTER_TOTAL_CANDIDATE_MESHLETS], mesh.meshletCount, globalMeshletIndex);

		uint clampedNumMeshlets = min(globalMeshletIndex + mesh.meshletCount, MAX_NUM_MESHLETS);
		uint numMeshletsToAdd = max(clampedNumMeshlets - globalMeshletIndex, 0);

		uint elementOffset;
		InterlockedAdd(candidateMeshletsCounter[COUNTER_PHASE1_CANDIDATE_MESHLETS], numMeshletsToAdd, elementOffset);
		for (uint i = 0; i < numMeshletsToAdd; ++i)
		{
			MeshletCandidate meshlet;
			meshlet.instanceID = instance.instanceId;
			meshlet.meshletIndex = i;
			candidateMeshlets[elementOffset + i] = meshlet;
		}
	}
}


struct BuildInstanceCullArgsConstants
{
	uint occludedInstancesCounterIdx;
	uint instanceCullArgsIdx;
};
ConstantBuffer<BuildInstanceCullArgsConstants> PassCB2 : register(b1);

[numthreads(1, 1, 1)]
void BuildInstanceCullArgsCS()
{
	RWBuffer<uint> occludedInstancesCounter = ResourceDescriptorHeap[PassCB2.occludedInstancesCounterIdx];
	RWStructuredBuffer<uint3> instanceCullArgs = ResourceDescriptorHeap[PassCB2.instanceCullArgsIdx];
	uint3 args = uint3(ceil(occludedInstancesCounter[0] * 1.0f / BLOCK_SIZE), 1, 1);
	instanceCullArgs[0] = args;
}