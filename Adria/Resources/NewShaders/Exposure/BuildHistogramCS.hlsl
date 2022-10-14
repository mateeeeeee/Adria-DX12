#include "CommonUtil.hlsli"
#include "ExposureUtil.hlsli"

struct PassConstants
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
ConstantBuffer<PassConstants> PassCB : register(b1);

SamplerState				  LinearClampSampler : register(s1);
groupshared uint			  HistogramBins[HISTOGRAM_BIN_NUM];

[numthreads(GROUP_SIZE_X, GROUP_SIZE_Y, 1)]
void BuildHistogramCS(uint groupIndex : SV_GroupIndex, uint3 dispatchThreadID : SV_DispatchThreadID)
{
	HistogramBins[groupIndex] = 0;
	GroupMemoryBarrierWithGroupSync();

	Texture2D SceneTexture = ResourceDescriptorHeap[PassCB.sceneIdx];
	RWByteAddressBuffer HistogramBuffer = ResourceDescriptorHeap[PassCB.histogramIdx];
	if (all(dispatchThreadID.xy < uint2(PassCB.width, PassCB.height)))
	{
		float2 screenPos = (float2) dispatchThreadID.xy + 0.5;
		float2 uv = screenPos * float2(PassCB.rcpWidth, PassCB.rcpHeight);
		float3 color = SceneTexture.SampleLevel(LinearClampSampler, uv, 0).xyz;

		float luminance = clamp(Luminance(color), PassCB.minLuminance, PassCB.maxLuminance);
		uint bin = GetHistogramBin(luminance, PassCB.minLuminance, PassCB.maxLuminance);
		float const weight = 1.0f;
		InterlockedAdd(HistogramBins[bin], uint(weight * 1024.0));
	}
	GroupMemoryBarrierWithGroupSync();
	HistogramBuffer.InterlockedAdd(groupIndex * 4, HistogramBins[groupIndex]);
}