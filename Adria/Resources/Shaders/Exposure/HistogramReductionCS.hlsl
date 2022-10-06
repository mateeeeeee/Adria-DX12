#include "ExposureUtil.hlsli"
#include "../Util/RootSignatures.hlsli"

struct ReduceHistogramConstants
{
	float minLuminance;
	float maxLuminance;
	float lowPercentile;
	float highPercentile;
};
ConstantBuffer<ReduceHistogramConstants> HistogramReductionCB : register(b0);
ByteAddressBuffer HistogramBuffer : register(t0);
RWTexture2D<float> AvgLuminanceTexture : register(u0);

groupshared float values[HISTOGRAM_BIN_NUM];
groupshared float intermediateValues[HISTOGRAM_BIN_NUM];

float ComputeAverageLuminance(float lowPercentileSum, float highPercentileSum)
{
	float sumLuminance = 0.0f;
	float sumWeight = 0.0f;

	[unroll]
	for (uint i = 0; i < HISTOGRAM_BIN_NUM; ++i)
	{
		float histogramValue = values[i];
		float luminance = GetLuminance(i, HistogramReductionCB.minLuminance, HistogramReductionCB.maxLuminance);

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

[RootSignature(HistogramReduction_RS)]
[numthreads(GROUP_SIZE_X, GROUP_SIZE_Y, 1)]
void main(uint groupIndex : SV_GroupIndex)
{
	float histogramValue = HistogramBuffer.Load(groupIndex * 4) / 1024.0f;

	values[groupIndex] = histogramValue;
	intermediateValues[groupIndex] = histogramValue;

	GroupMemoryBarrierWithGroupSync();

	[unroll]
	for (uint i = (HISTOGRAM_BIN_NUM >> 1); i > 0; i >>= 1)
	{
		if (groupIndex < i)
		{
			intermediateValues[groupIndex] += intermediateValues[groupIndex + i];
		}

		GroupMemoryBarrierWithGroupSync();
	}

	float histogramValueSum = intermediateValues[0];

	if (groupIndex == 0)
	{
		AvgLuminanceTexture[uint2(0, 0)] = ComputeAverageLuminance(histogramValueSum * HistogramReductionCB.lowPercentile, histogramValueSum * HistogramReductionCB.highPercentile);
	}
}