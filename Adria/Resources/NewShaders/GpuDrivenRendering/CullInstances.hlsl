#include "GpuDrivenRendering.hlsli"
#include "../CommonResources.hlsli"
#include "../Common.hlsli"
#include "../Scene.hlsli"

#define BLOCK_SIZE 64

struct CullInstances1stPhaseConstants
{
	uint numInstances;
	uint hzbIdx;
	uint occludedInstancesIdx;
	uint occludedInstancesCounterIdx;
	uint candidateMeshletsIdx;
	uint candidateMeshletsCounterIdx;
};
ConstantBuffer<CullInstances1stPhaseConstants> PassCB : register(b1);

[numthreads(BLOCK_SIZE, 1, 1)]
void CullInstances1stPhaseCS(uint threadId : SV_DispatchThreadID)
{
	if (threadId >= PassCB.numInstances) return;

	Instance instance = GetInstanceData(threadId);
	Mesh mesh = GetMeshData(instance.meshIndex);

	FrustumCullData cullData = FrustumCull(instance.bbOrigin, instance.bbExtents, instance.worldMatrix, FrameCB.viewProjection);
	bool isVisible = cullData.isVisible;
	bool wasOccluded = false;

#ifdef OCCLUSION_CULL
	if (isVisible)
	{
		FrustumCullData prevCullData = FrustumCull(instance.bbOrigin, instance.bbExtents, instance.worldMatrix, FrameCB.prevViewProjection);
		Texture2D<float> hzbTx = ResourceDescriptorHeap[PassCB.hzbIdx];
		wasOccluded = !HZBCull(prevCullData, hzbTx);
		if (wasOccluded)
		{
			RWBuffer<uint> occludedInstancesCounter = ResourceDescriptorHeap[PassCB.occludedInstancesCounterIdx];
			RWStructuredBuffer<uint> occludedInstances = ResourceDescriptorHeap[PassCB.occludedInstancesIdx];
			uint elementOffset = 0;
			InterlockedAdd(occludedInstancesCounter[0], 1, elementOffset);
			occludedInstances[elementOffset] = instance.instanceId;
		}
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