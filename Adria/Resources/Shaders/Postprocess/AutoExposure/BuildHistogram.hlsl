#include "Common.hlsli"
#include "ExposureUtil.hlsli"

struct BuildHistogramConstants
{
	uint  width;
	uint  height;
	float rcpWidth;
	float rcpHeight;
	float minLogLuminance;
	float logLuminanceRangeRcp;
	uint  sceneIdx;
	uint  histogramIdx;
};
ConstantBuffer<BuildHistogramConstants> BuildHistogramPassCB : register(b1);

SamplerState				  LinearClampSampler : register(s1);
groupshared uint			  SharedHistogramBins[HISTOGRAM_BIN_NUM];

[numthreads(GROUP_SIZE_X, GROUP_SIZE_Y, 1)]
void BuildHistogramCS(uint GroupIndex : SV_GroupIndex, uint3 DispatchThreadId : SV_DispatchThreadID)
{
	SharedHistogramBins[GroupIndex] = 0;
	GroupMemoryBarrierWithGroupSync();

	Texture2D sceneTexture = ResourceDescriptorHeap[BuildHistogramPassCB.sceneIdx];
	if (all(DispatchThreadId.xy < uint2(BuildHistogramPassCB.width, BuildHistogramPassCB.height)))
	{
		float2 screenPos = (float2) DispatchThreadId.xy + 0.5;
		float2 uv = screenPos * float2(BuildHistogramPassCB.rcpWidth, BuildHistogramPassCB.rcpHeight);
		float3 color = sceneTexture.SampleLevel(LinearClampSampler, uv, 0).xyz;
		uint bin = GetHistogramBin(color, BuildHistogramPassCB.minLogLuminance, BuildHistogramPassCB.logLuminanceRangeRcp);
		InterlockedAdd(SharedHistogramBins[bin], 1);
	}
	GroupMemoryBarrierWithGroupSync();

	RWByteAddressBuffer HistogramBuffer = ResourceDescriptorHeap[BuildHistogramPassCB.histogramIdx];
	HistogramBuffer.InterlockedAdd(GroupIndex * 4, SharedHistogramBins[GroupIndex]);
}