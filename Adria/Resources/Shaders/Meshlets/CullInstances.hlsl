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


struct CullInstancesConstants
{
	uint numInstances;
	uint hzbIdx;
	uint occludedInstancesIdx;
	uint occludedInstancesCounterIdx;
	uint candidateMeshletsIdx;
	uint candidateMeshletsCounterIdx;
};
ConstantBuffer<CullInstancesConstants> CullInstancesPassCB : register(b1);

[numthreads(BLOCK_SIZE, 1, 1)]
void CullInstancesCS(uint ThreadId : SV_DispatchThreadID)
{
#if !SECOND_PHASE
	uint numInstances = CullInstancesPassCB.numInstances;
	uint instanceIndex = ThreadId;
#else
	Buffer<uint> occludedInstancesCounter = ResourceDescriptorHeap[CullInstancesPassCB.occludedInstancesCounterIdx];
	StructuredBuffer<uint> occludedInstancesBuffer = ResourceDescriptorHeap[CullInstancesPassCB.occludedInstancesIdx];
	uint numInstances = occludedInstancesCounter[0];
	uint instanceIndex = occludedInstancesBuffer[ThreadId];
#endif
	if (ThreadId >= numInstances) return;

	Instance instance = GetInstanceData(instanceIndex);
	Mesh mesh = GetMeshData(instance.meshIndex);

	FrustumCullData cullData = FrustumCull(instance.bbOrigin, instance.bbExtents, instance.worldMatrix, FrameCB.viewProjection);
	bool isVisible = cullData.isVisible;
	bool wasOccluded = false;

#if SKIP_ALPHA_BLENDED
	Material material = GetMaterialData(instance.materialIdx);
	if (material.alphaBlended)
	{
		isVisible = false;
	}
#endif

#if OCCLUSION_CULL
	if (isVisible)
	{
		Texture2D<float> hzbTexture = ResourceDescriptorHeap[CullInstancesPassCB.hzbIdx];
#if !SECOND_PHASE
		FrustumCullData prevCullData = FrustumCull(instance.bbOrigin, instance.bbExtents, instance.worldMatrix, FrameCB.prevViewProjection);
		if (prevCullData.isVisible)
		{
			wasOccluded = !HZBCull(prevCullData, hzbTexture);
		}

		if (wasOccluded)
		{
			RWBuffer<uint> occludedInstancesCounter = ResourceDescriptorHeap[CullInstancesPassCB.occludedInstancesCounterIdx];
			RWStructuredBuffer<uint> occludedInstancesBuffer = ResourceDescriptorHeap[CullInstancesPassCB.occludedInstancesIdx];
			uint elementOffset = 0;
			InterlockedAdd(occludedInstancesCounter[0], 1, elementOffset);
			occludedInstancesBuffer[elementOffset] = instance.instanceId;
		}
#else
		isVisible = HZBCull(cullData, hzbTexture);
#endif
	}
#endif


	if (isVisible && !wasOccluded)
	{
		RWBuffer<uint> candidateMeshletsCounter = ResourceDescriptorHeap[CullInstancesPassCB.candidateMeshletsCounterIdx];
		RWStructuredBuffer<MeshletCandidate> candidateMeshletsBuffer = ResourceDescriptorHeap[CullInstancesPassCB.candidateMeshletsIdx];

		uint globalMeshletIndex;
		InterlockedAdd(candidateMeshletsCounter[COUNTER_TOTAL_CANDIDATE_MESHLETS], mesh.meshletCount, globalMeshletIndex);

		uint clampedNumMeshlets = min(globalMeshletIndex + mesh.meshletCount, MAX_NUM_MESHLETS);
		uint numMeshletsToAdd = max(clampedNumMeshlets - globalMeshletIndex, 0);

		uint elementOffset;
		InterlockedAdd(candidateMeshletsCounter[COUNTER_PHASE1_CANDIDATE_MESHLETS], numMeshletsToAdd, elementOffset);

#if SECOND_PHASE
		elementOffset += candidateMeshletsCounter[COUNTER_PHASE1_VISIBLE_MESHLETS];
#endif
		for (uint i = 0; i < numMeshletsToAdd; ++i)
		{
			MeshletCandidate meshlet;
			meshlet.instanceID = instance.instanceId;
			meshlet.meshletIndex = i;
			candidateMeshletsBuffer[elementOffset + i] = meshlet;
		}
	}
}


struct BuildInstanceCullArgsConstants
{
	uint occludedInstancesCounterIdx;
	uint instanceCullArgsIdx;
};
ConstantBuffer<BuildInstanceCullArgsConstants> BuildInstanceCullArgsPassCB : register(b1);

[numthreads(1, 1, 1)]
void BuildInstanceCullArgsCS()
{
	RWBuffer<uint> occludedInstancesCounter = ResourceDescriptorHeap[BuildInstanceCullArgsPassCB.occludedInstancesCounterIdx];
	RWStructuredBuffer<uint3> instanceCullArgsBuffer = ResourceDescriptorHeap[BuildInstanceCullArgsPassCB.instanceCullArgsIdx];
	uint3 args = uint3(ceil(occludedInstancesCounter[0] * 1.0f / BLOCK_SIZE), 1, 1);
	instanceCullArgsBuffer[0] = args;
}