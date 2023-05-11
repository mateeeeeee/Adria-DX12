#include "GpuDrivenRendering.hlsli"

struct BuildMeshletCullArgsConstants
{
	uint candidateMeshletsCounterIdx;
	uint meshletCullArgsIdx;
};
ConstantBuffer<BuildMeshletCullArgsConstants> PassCB : register(b1);


[numthreads(1, 1, 1)]
void BuildMeshletCullArgsCS()
{
	RWBuffer<uint> candidateMeshletsCounter = ResourceDescriptorHeap[PassCB.candidateMeshletsCounterIdx];
	RWStructuredBuffer<uint3> meshletCullArgs = ResourceDescriptorHeap[PassCB.meshletCullArgsIdx];

#ifdef FIRST_PHASE
	uint numMeshlets = candidateMeshletsCounter[COUNTER_PHASE1_CANDIDATE_MESHLETS];
#else
	uint numMeshlets = candidateMeshletsCounter[COUNTER_PHASE2_CANDIDATE_MESHLETS];
#endif
	uint3 args = uint3(ceil(numMeshlets / 64.0f), 1, 1);
	meshletCullArgs[0] = args;
}
