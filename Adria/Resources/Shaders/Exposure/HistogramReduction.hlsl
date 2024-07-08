#include "Common.hlsli"
#include "ExposureUtil.hlsli"


struct HistogramReductionConstants
{
	float minLuminance;
	float maxLuminance;
	float lowPercentile;
	float highPercentile;
	uint  histogramIdx;
	uint  luminanceIdx;
};
ConstantBuffer<HistogramReductionConstants> HistogramReductionPassCB : register(b1);

groupshared float SharedValues[HISTOGRAM_BIN_NUM];
groupshared float IntermediateSharedValues[HISTOGRAM_BIN_NUM];

float ComputeAverageLuminance(float lowPercentileSum, float highPercentileSum)
{
	float sumLuminance = 0.0f;
	float sumWeight = 0.0f;

	[unroll]
	for (uint i = 0; i < HISTOGRAM_BIN_NUM; ++i)
	{
		float histogramValue = SharedValues[i];
		float luminance = GetLuminance(i, HistogramReductionPassCB.minLuminance, HistogramReductionPassCB.maxLuminance);

		// remove dark values
		float off = min(lowPercentileSum, histogramValue);
		lowPercentileSum -= off;
		highPercentileSum -= off;
		histogramValue -= off;

		// remove highlight values
		histogramValue = min(highPercentileSum, histogramValue);
		highPercentileSum -= histogramValue;

		sumLuminance += histogramValue * luminance;
		sumWeight += histogramValue;
	}
	return sumLuminance / max(sumWeight, 0.0001);
}

[numthreads(GROUP_SIZE_X, GROUP_SIZE_Y, 1)]
void HistogramReductionCS(uint GroupIndex : SV_GroupIndex)
{
	ByteAddressBuffer  histogramBuffer = ResourceDescriptorHeap[HistogramReductionPassCB.histogramIdx];
	RWTexture2D<float> averageLuminanceTexture = ResourceDescriptorHeap[HistogramReductionPassCB.luminanceIdx];

	float histogramValue = histogramBuffer.Load(GroupIndex * 4);
	SharedValues[GroupIndex] = histogramValue;
	IntermediateSharedValues[GroupIndex] = histogramValue;
	GroupMemoryBarrierWithGroupSync();
	[unroll]
	for (uint i = (HISTOGRAM_BIN_NUM >> 1); i > 0; i >>= 1)
	{
		if (GroupIndex < i)
		{
			IntermediateSharedValues[GroupIndex] += IntermediateSharedValues[GroupIndex + i];
		}
		GroupMemoryBarrierWithGroupSync();
	}
	float histogramValueSum = IntermediateSharedValues[0];
	if (GroupIndex == 0)
	{
		averageLuminanceTexture[uint2(0, 0)] = ComputeAverageLuminance(histogramValueSum * HistogramReductionPassCB.lowPercentile, histogramValueSum * HistogramReductionPassCB.highPercentile);
	}
}