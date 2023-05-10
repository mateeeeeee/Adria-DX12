#include "GpuDrivenRendering.hlsli"
#include "../CommonResources.hlsli"
#include "../Common.hlsli"
#include "../Scene.hlsli"

#define BLOCK_SIZE 64

struct CullMeshlets1stPhaseConstants
{
	uint hzbIdx;
	uint candidateMeshletsIdx;
	uint candidateMeshletsCounterIdx;
	uint visibleMeshletsIdx;
	uint visibleMeshletsCounterIdx;
};
ConstantBuffer<CullMeshlets1stPhaseConstants> PassCB : register(b1);

[numthreads(BLOCK_SIZE, 1, 1)]
void CullMeshlets1stPhaseCS(uint threadId : SV_DispatchThreadID)
{
	RWBuffer<uint> candidateMeshletsCounter = ResourceDescriptorHeap[PassCB.candidateMeshletsCounterIdx];
	RWStructuredBuffer<MeshletCandidate> candidateMeshlets = ResourceDescriptorHeap[PassCB.candidateMeshletsIdx];
	RWBuffer<uint> visibleMeshletsCounter = ResourceDescriptorHeap[PassCB.visibleMeshletsCounterIdx];
	RWStructuredBuffer<MeshletCandidate> visibleMeshlets = ResourceDescriptorHeap[PassCB.visibleMeshletsIdx];

	if (threadId >= candidateMeshletsCounter[COUNTER_PHASE1_CANDIDATE_MESHLETS]) return;

	uint candidateIndex = threadId;
	MeshletCandidate candidate = candidateMeshlets[candidateIndex];
	Instance instance = GetInstanceData(candidate.instanceID);
	Mesh mesh = GetMeshData(instance.meshIndex);
	Meshlet meshlet = GetMeshletData(mesh.bufferIdx, mesh.meshletOffset, candidate.meshletIndex);
	FrustumCullData cullData = FrustumCull(meshlet.center, meshlet.radius.xxx, instance.worldMatrix, FrameCB.viewProjection);
	bool isVisible = cullData.isVisible;
	bool wasOccluded = false;

#ifdef OCCLUSION_CULL
	if (isVisible)
	{
		FrustumCullData prevCullData = FrustumCull(meshlet.center, meshlet.radius.xxx, instance.worldMatrix, FrameCB.prevViewProjection);
		if (prevCullData.isVisible)
		{
			Texture2D<float> hzbTx = ResourceDescriptorHeap[PassCB.hzbIdx];
			wasOccluded = !HZBCull(prevCullData, hzbTx);
		}

		if (wasOccluded)
		{
			uint globalMeshletIndex;
			InterlockedAdd(candidateMeshletsCounter[COUNTER_TOTAL_CANDIDATE_MESHLETS], 1, globalMeshletIndex);
			if (globalMeshletIndex < MAX_NUM_MESHLETS)
			{
				uint elementOffset;
				InterlockedAdd(candidateMeshletsCounter[COUNTER_PHASE2_CANDIDATE_MESHLETS], 1, elementOffset);
				uint meshletOffset = candidateMeshletsCounter[COUNTER_PHASE1_CANDIDATE_MESHLETS];
				candidateMeshlets[meshletOffset + elementOffset] = candidate;
			}
		}
	}
#endif

	if (isVisible && !wasOccluded)
	{
		uint elementOffset;
		InterlockedAdd(visibleMeshletsCounter[COUNTER_PHASE1_VISIBLE_MESHLETS], 1, elementOffset);
		visibleMeshlets[elementOffset] = candidate;
	}

}