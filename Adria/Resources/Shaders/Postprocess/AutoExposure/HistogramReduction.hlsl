#include "Common.hlsli"
#include "ExposureUtil.hlsli"


struct HistogramReductionConstants
{
	float minLogLuminance;
	float logLuminanceRange;
	float deltaTime;
	float adaptionSpeed;
	uint  pixelCount;
	uint  histogramIdx;
	uint  luminanceIdx;
	uint  exposureIdx;
};
ConstantBuffer<HistogramReductionConstants> HistogramReductionPassCB : register(b1);
groupshared float HistogramShared[HISTOGRAM_BIN_NUM];

[numthreads(GROUP_SIZE_X, GROUP_SIZE_Y, 1)]
void HistogramReductionCS(uint GroupIndex : SV_GroupIndex)
{
	ByteAddressBuffer  histogramBuffer = ResourceDescriptorHeap[HistogramReductionPassCB.histogramIdx];
	RWTexture2D<float> averageLuminanceTexture = ResourceDescriptorHeap[HistogramReductionPassCB.luminanceIdx];
	RWTexture2D<float> exposureTexture = ResourceDescriptorHeap[HistogramReductionPassCB.exposureIdx];

	float histogramValue = histogramBuffer.Load(GroupIndex * 4);
	HistogramShared[GroupIndex] = histogramValue * GroupIndex;
	GroupMemoryBarrierWithGroupSync();

	[unroll]
	for (uint i = (HISTOGRAM_BIN_NUM >> 1); i > 0; i >>= 1)
	{
		if (GroupIndex < i)
		{
			HistogramShared[GroupIndex] += HistogramShared[GroupIndex + i];
		}
		GroupMemoryBarrierWithGroupSync();
	}

	if(GroupIndex == 0)
	{
		float sharedValue = HistogramShared[0];
		float pixelCount = HistogramReductionPassCB.pixelCount;

		float weightedLogAverage = (sharedValue / max((float)pixelCount - histogramValue, 1.0)) - 1.0;
		float weightedAverageLuminance = exp2(((weightedLogAverage / (HISTOGRAM_BIN_NUM - 1)) * HistogramReductionPassCB.logLuminanceRange) + HistogramReductionPassCB.minLogLuminance);
		float luminanceHistory = averageLuminanceTexture[uint2(0, 0)];
		float adaptedLuminance = Adaption(luminanceHistory, weightedAverageLuminance, HistogramReductionPassCB.deltaTime, HistogramReductionPassCB.adaptionSpeed);
		averageLuminanceTexture[uint2(0, 0)] = adaptedLuminance;	

		float EV100 = ComputeEV100(adaptedLuminance);
		float exposure = ComputeExposure(EV100);
		exposureTexture[uint2(0, 0)] = exposure;	
	}
}