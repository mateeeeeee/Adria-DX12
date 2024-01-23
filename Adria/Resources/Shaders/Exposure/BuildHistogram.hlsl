#include "Common.hlsli"
#include "ExposureUtil.hlsli"

struct BuildHistogramConstants
{
	uint  width;
	uint  height;
	float rcpWidth;
	float rcpHeight;
	float minLuminance;
	float maxLuminance;
	uint  sceneIdx;
	uint  histogramIdx;
};
ConstantBuffer<BuildHistogramConstants> PassCB : register(b1);

SamplerState				  LinearClampSampler : register(s1);
groupshared uint			  HistogramBins[HISTOGRAM_BIN_NUM];

[numthreads(GROUP_SIZE_X, GROUP_SIZE_Y, 1)]
void BuildHistogramCS(uint groupIndex : SV_GroupIndex, uint3 dispatchThreadId : SV_DispatchThreadID)
{
	HistogramBins[groupIndex] = 0;
	GroupMemoryBarrierWithGroupSync();

	Texture2D SceneTexture = ResourceDescriptorHeap[PassCB.sceneIdx];
	if (all(dispatchThreadId.xy < uint2(PassCB.width, PassCB.height)))
	{
		float2 screenPos = (float2) dispatchThreadId.xy + 0.5;
		float2 uv = screenPos * float2(PassCB.rcpWidth, PassCB.rcpHeight);
		float3 color = SceneTexture.SampleLevel(LinearClampSampler, uv, 0).xyz;

		float luminance = clamp(Luminance(color), PassCB.minLuminance, PassCB.maxLuminance);
		uint bin = GetHistogramBin(luminance, PassCB.minLuminance, PassCB.maxLuminance);
		InterlockedAdd(HistogramBins[bin], 1);
	}
	GroupMemoryBarrierWithGroupSync();

	RWByteAddressBuffer HistogramBuffer = ResourceDescriptorHeap[PassCB.histogramIdx];
	HistogramBuffer.InterlockedAdd(groupIndex * 4, HistogramBins[groupIndex]);
}