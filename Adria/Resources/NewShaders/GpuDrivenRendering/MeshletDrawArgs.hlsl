#include "GpuDrivenRendering.hlsli"

struct BuildMeshletDrawArgsConstants
{
	uint visibleMeshletsCounterIdx;
	uint meshletDrawArgsIdx;
};
ConstantBuffer<BuildMeshletDrawArgsConstants> PassCB : register(b1);


[numthreads(1, 1, 1)]
void BuildMeshletDrawArgsCS()
{
	RWBuffer<uint> visibleMeshletsCounter = ResourceDescriptorHeap[PassCB.visibleMeshletsCounterIdx];
	RWStructuredBuffer<uint3> meshletDrawArgs = ResourceDescriptorHeap[PassCB.meshletDrawArgsIdx];

#ifdef FIRST_PHASE
	uint numMeshlets = visibleMeshletsCounter[COUNTER_PHASE1_VISIBLE_MESHLETS];
#else
	uint numMeshlets = visibleMeshletsCounter[COUNTER_PHASE2_VISIBLE_MESHLETS];
#endif
	uint3 args = uint3(ceil(numMeshlets / 1), 1, 1);
	meshletDrawArgs[0] = args;
}