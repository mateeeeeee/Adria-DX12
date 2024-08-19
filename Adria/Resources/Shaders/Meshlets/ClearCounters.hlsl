
struct ClearCountersConstants
{
	uint candidateMeshletsCounterIdx;
	uint visibleMeshletsCounterIdx;
	uint occludedInstancesCounterIdx;
};

ConstantBuffer<ClearCountersConstants> ClearCountersPassCB : register(b1);

[numthreads(1, 1, 1)]
void ClearCountersCS()
{
	RWBuffer<uint> candidateMeshletsCounter = ResourceDescriptorHeap[ClearCountersPassCB.candidateMeshletsCounterIdx];
	RWBuffer<uint> visibleMeshletsCounter = ResourceDescriptorHeap[ClearCountersPassCB.visibleMeshletsCounterIdx];
	RWBuffer<uint> occludedInstancesCounter = ResourceDescriptorHeap[ClearCountersPassCB.occludedInstancesCounterIdx];

	candidateMeshletsCounter[0] = 0;
	candidateMeshletsCounter[1] = 0;
	candidateMeshletsCounter[2] = 0;

	visibleMeshletsCounter[0] = 0;
	visibleMeshletsCounter[1] = 0;

	occludedInstancesCounter[0] = 0;
}