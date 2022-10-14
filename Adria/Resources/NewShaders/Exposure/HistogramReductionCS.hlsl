#include "CommonUtil.hlsli"
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
ConstantBuffer<HistogramReductionConstants> PassCB : register(b1);

groupshared float Values[HISTOGRAM_BIN_NUM];
groupshared float IntermediateValues[HISTOGRAM_BIN_NUM];

float ComputeAverageLuminance(float lowPercentileSum, float highPercentileSum)
{
	float sumLuminance = 0.0f;
	float sumWeight = 0.0f;

	[unroll]
	for (uint i = 0; i < HISTOGRAM_BIN_NUM; ++i)
	{
		float histogramValue = Values[i];
		float luminance = GetLuminance(i, PassCB.minLuminance, PassCB.maxLuminance);

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
void HistogramReductionCS(uint groupIndex : SV_GroupIndex)
{
	ByteAddressBuffer  HistogramBuffer = ResourceDescriptorHeap[PassCB.histogramIdx];
	RWTexture2D<float> AvgLuminanceTexture = ResourceDescriptorHeap[PassCB.luminanceIdx];

	float histogramValue = HistogramBuffer.Load(groupIndex * 4) / 1024.0f;
	Values[groupIndex] = histogramValue;
	IntermediateValues[groupIndex] = histogramValue;
	GroupMemoryBarrierWithGroupSync();
	[unroll]
	for (uint i = (HISTOGRAM_BIN_NUM >> 1); i > 0; i >>= 1)
	{
		if (groupIndex < i)
		{
			IntermediateValues[groupIndex] += IntermediateValues[groupIndex + i];
		}
		GroupMemoryBarrierWithGroupSync();
	}
	float histogramValueSum = IntermediateValues[0];
	if (groupIndex == 0)
	{
		AvgLuminanceTexture[uint2(0, 0)] = ComputeAverageLuminance(histogramValueSum * PassCB.lowPercentile, histogramValueSum * PassCB.highPercentile);
	}
}