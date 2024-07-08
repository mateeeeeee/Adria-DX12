#include "DDGICommon.hlsli"
#include "Primitives.hlsli"
#include "CommonResources.hlsli"
#include "Common.hlsli"

#define DDGI_VISUALIZE_IRRADIANCE 0
#define DDGI_VISUALIZE_DISTANCE   1

struct DDGIVisualizeConstants
{
    uint visualizeMode;
};
ConstantBuffer<DDGIVisualizeConstants> DDGIVisualizePassCB : register(b1);


struct VSToPS
{
	float4 Position     : SV_POSITION;
	float3 Normal		: NORMAL;
	uint   ProbeIndex	: PROBE_INDEX;
};

VSToPS DDGIVisualizeVS(uint VertexID : SV_VertexID, uint InstanceID : SV_InstanceID)
{
	VSToPS output = (VSToPS)0;

	StructuredBuffer<DDGIVolume> ddgiVolumeBuffer = ResourceDescriptorHeap[FrameCB.ddgiVolumesIdx];
	DDGIVolume ddgiVolume = ddgiVolumeBuffer[0];

	uint probeIdx = InstanceID;
	float3 probePosition = GetProbeLocation(ddgiVolume, probeIdx);
	float3 sphereVertex = SphereVertices[VertexID].xyz;
	const float sphereRadius = 1.0f;
	float3 worldPos = probePosition + sphereRadius * sphereVertex;

	output.Position = mul(float4(worldPos, 1), FrameCB.viewProjection);
	output.Normal = sphereVertex;
	output.ProbeIndex = probeIdx;

	return output;
}


float4 DDGIVisualizePS(VSToPS input) : SV_TARGET
{
	StructuredBuffer<DDGIVolume> ddgiVolumeBuffer = ResourceDescriptorHeap[FrameCB.ddgiVolumesIdx];
	DDGIVolume ddgiVolume = ddgiVolumeBuffer[0];

	uint3 gridCoord = GetProbeGridCoord(ddgiVolume, input.ProbeIndex);
	float3 probePosition = GetProbeLocationFromGridCoord(ddgiVolume, gridCoord);

	float4 result = 0.0f;
	if (DDGIVisualizePassCB.visualizeMode == DDGI_VISUALIZE_IRRADIANCE)
	{
		float2 uv = GetProbeUV(ddgiVolume, gridCoord, input.Normal, PROBE_IRRADIANCE_TEXELS);
		Texture2D<float4> irradianceTexture = ResourceDescriptorHeap[ddgiVolume.irradianceHistoryIdx];
		float3 radiance = irradianceTexture.SampleLevel(LinearClampSampler, uv, 0).rgb;
		radiance = pow(radiance, 2.5f);
		float3 color = radiance / M_PI;
		result = float4(color, 1.0f);
	}
	else if (DDGIVisualizePassCB.visualizeMode == DDGI_VISUALIZE_DISTANCE)
	{
		float2 uv = GetProbeUV(ddgiVolume, gridCoord, input.Normal, PROBE_DISTANCE_TEXELS);
		Texture2D<float2> distanceTexture = ResourceDescriptorHeap[ddgiVolume.distanceHistoryIdx];
		float distance = distanceTexture.SampleLevel(LinearClampSampler, uv, 0).r;
		float3 color = distance.xxx / (Max(ddgiVolume.probeSize) * 3);
		result = float4(color, 1.0f);
	}
	return result;
}