#include "GpuDrivenRendering.hlsli"
#include "CommonResources.hlsli"
#include "Common.hlsli"
#include "Scene.hlsli"

#define BLOCK_SIZE 64

#ifndef SECOND_PHASE
#define SECOND_PHASE 0
#endif

#ifndef OCCLUSION_CULL
#define OCCLUSION_CULL 1
#endif


struct CullMeshletsConstants
{
	uint hzbIdx;
	uint candidateMeshletsIdx;
	uint candidateMeshletsCounterIdx;
	uint visibleMeshletsIdx;
	uint visibleMeshletsCounterIdx;
};
ConstantBuffer<CullMeshletsConstants> PassCB : register(b1);



[numthreads(BLOCK_SIZE, 1, 1)]
void CullMeshletsCS(uint threadId : SV_DispatchThreadID)
{
	RWBuffer<uint> candidateMeshletsCounter = ResourceDescriptorHeap[PassCB.candidateMeshletsCounterIdx];
	RWStructuredBuffer<MeshletCandidate> candidateMeshlets = ResourceDescriptorHeap[PassCB.candidateMeshletsIdx];
	RWBuffer<uint> visibleMeshletsCounter = ResourceDescriptorHeap[PassCB.visibleMeshletsCounterIdx];
	RWStructuredBuffer<MeshletCandidate> visibleMeshlets = ResourceDescriptorHeap[PassCB.visibleMeshletsIdx];

#if SECOND_PHASE
	uint candidateMeshletCounterIdx = COUNTER_PHASE2_CANDIDATE_MESHLETS;
	uint candidateIndex = threadId + candidateMeshletsCounter[COUNTER_PHASE1_CANDIDATE_MESHLETS];
#else
	uint candidateMeshletCounterIdx = COUNTER_PHASE1_CANDIDATE_MESHLETS;
	uint candidateIndex = threadId;
#endif

	if (threadId >= candidateMeshletsCounter[candidateMeshletCounterIdx]) return;

	MeshletCandidate candidate = candidateMeshlets[candidateIndex];
	Instance instance = GetInstanceData(candidate.instanceID);
	Mesh mesh = GetMeshData(instance.meshIndex);
	Meshlet meshlet = GetMeshletData(mesh.bufferIdx, mesh.meshletOffset, candidate.meshletIndex);
	FrustumCullData cullData = FrustumCull(meshlet.center, meshlet.radius.xxx, instance.worldMatrix, FrameCB.viewProjection);
	bool isVisible = cullData.isVisible;
	bool wasOccluded = false;

#if OCCLUSION_CULL
	if (isVisible)
	{
		Texture2D<float> hzbTx = ResourceDescriptorHeap[PassCB.hzbIdx];
#if !SECOND_PHASE
		FrustumCullData prevCullData = FrustumCull(meshlet.center, meshlet.radius.xxx, instance.worldMatrix, FrameCB.prevViewProjection);
		if (prevCullData.isVisible)
		{
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
#else
		isVisible = HZBCull(cullData, hzbTx);
#endif
	}
#endif

	if (isVisible && !wasOccluded)
	{
		uint elementOffset;
		InterlockedAdd(visibleMeshletsCounter[COUNTER_PHASE1_VISIBLE_MESHLETS], 1, elementOffset);
#if SECOND_PHASE
		elementOffset += visibleMeshletsCounter[COUNTER_PHASE1_VISIBLE_MESHLETS];
#endif
		visibleMeshlets[elementOffset] = candidate;
	}

}

struct BuildMeshletCullArgsConstants
{
	uint candidateMeshletsCounterIdx;
	uint meshletCullArgsIdx;
};
ConstantBuffer<BuildMeshletCullArgsConstants> PassCB2 : register(b1);


[numthreads(1, 1, 1)]
void BuildMeshletCullArgsCS()
{
	RWBuffer<uint> candidateMeshletsCounter = ResourceDescriptorHeap[PassCB2.candidateMeshletsCounterIdx];
	RWStructuredBuffer<uint3> meshletCullArgs = ResourceDescriptorHeap[PassCB2.meshletCullArgsIdx];

#if !SECOND_PHASE
	uint numMeshlets = candidateMeshletsCounter[COUNTER_PHASE1_CANDIDATE_MESHLETS];
#else
	uint numMeshlets = candidateMeshletsCounter[COUNTER_PHASE2_CANDIDATE_MESHLETS];
#endif
	uint3 args = uint3(ceil(numMeshlets / 64.0f), 1, 1);
	meshletCullArgs[0] = args;
}