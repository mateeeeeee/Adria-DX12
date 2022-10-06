#include "ExposureUtil.hlsli"
#include "../Util/RootSignatures.hlsli"

struct BuildHistogramConstants
{
	uint width;
	uint height;
	float rcpWidth;
	float rcpHeight;
	float minLuminance;
	float maxLuminance;
};
ConstantBuffer<BuildHistogramConstants> BuildHistogramCB : register(b0);

Texture2D			SceneTexture	: register(t0);
SamplerState		LinearClampSampler : register(s0);
RWByteAddressBuffer HistogramBuffer : register(u0);

groupshared uint HistogramBins[HISTOGRAM_BIN_NUM];

[RootSignature(BuildHistogram_RS)]
[numthreads(GROUP_SIZE_X, GROUP_SIZE_Y, 1)]
void main(uint groupIndex : SV_GroupIndex, uint3 dispatchThreadID : SV_DispatchThreadID)
{
	HistogramBins[groupIndex] = 0;
	GroupMemoryBarrierWithGroupSync();

	if (all(dispatchThreadID.xy < uint2(BuildHistogramCB.width, BuildHistogramCB.height)))
	{
		float2 screenPos = (float2) dispatchThreadID.xy + 0.5;
		float2 uv = screenPos * float2(BuildHistogramCB.rcpWidth, BuildHistogramCB.rcpHeight);
		float3 color = SceneTexture.SampleLevel(LinearClampSampler, uv, 0).xyz;

		float luminance = clamp(Luminance(color), BuildHistogramCB.minLuminance, BuildHistogramCB.maxLuminance);
		uint bin = GetHistogramBin(luminance, BuildHistogramCB.minLuminance, BuildHistogramCB.maxLuminance);
		float const weight = 1.0f;

		InterlockedAdd(HistogramBins[bin], uint(weight * 1024.0));
	}
	GroupMemoryBarrierWithGroupSync();

	HistogramBuffer.InterlockedAdd(groupIndex * 4, HistogramBins[groupIndex]);
}