#include "DDGICommon.hlsli"
#include "Common.hlsli"

struct DDGIPassConstants
{
	float3 randomVector;
	float  randomAngle;
	float  historyBlendWeight;
	uint   rayBufferIdx;
	uint   irradianceIdx;
};
ConstantBuffer<DDGIPassConstants> PassCB : register(b1);

#define CACHE_SIZE PROBE_IRRADIANCE_TEXELS * PROBE_IRRADIANCE_TEXELS
groupshared float3 RadianceCache[CACHE_SIZE];
groupshared float3 DirectionCache[CACHE_SIZE];

struct CSInput
{
    uint3 GroupId : SV_GroupID;
    uint3 GroupThreadId : SV_GroupThreadID;
    uint3 DispatchThreadId : SV_DispatchThreadID;
    uint  GroupIndex : SV_GroupIndex;
};

static const uint BORDER_TEXELS = PROBE_IRRADIANCE_TEXELS * 4 + 4;
static const uint4 BORDER_OFFSETS[BORDER_TEXELS] = 
{
	uint4(6, 6, 0, 0), 
	uint4(6, 1, 1, 0), uint4(5, 1, 2, 0), uint4(4, 1, 3, 0), uint4(3, 1, 4, 0), uint4(2, 1, 5, 0), uint4(1, 1, 6, 0),
	uint4(1, 6, 7, 0),
	uint4(1, 6, 0, 1), uint4(1, 5, 0, 2), uint4(1, 4, 0, 3), uint4(1, 3, 0, 4), uint4(1, 2, 0, 5), uint4(1, 1, 0, 6),
	uint4(6, 1, 0, 7),
	uint4(6, 6, 7, 1), uint4(6, 5, 7, 2), uint4(6, 4, 7, 3), uint4(6, 3, 7, 4), uint4(6, 2, 7, 5), uint4(6, 1, 7, 6),
	uint4(1, 1, 7, 7),
	uint4(6, 6, 1, 7), uint4(5, 6, 2, 7), uint4(4, 6, 3, 7), uint4(3, 6, 4, 7), uint4(2, 6, 5, 7), uint4(1, 6, 6, 7) 
};

[numthreads(PROBE_IRRADIANCE_TEXELS, PROBE_IRRADIANCE_TEXELS, 1)]
void DDGI_UpdateIrradiance(CSInput input)
{
	StructuredBuffer<DDGIVolume> ddgiVolumes = ResourceDescriptorHeap[FrameCB.ddgiVolumesIdx];
	DDGIVolume ddgiVolume = ddgiVolumes[0];

	uint  probeIdx = input.GroupId.x;
	uint3 probeGridCoords = GetProbeGridCoord(ddgiVolume, probeIdx);
	uint2 texelLocation = GetProbeTexel(ddgiVolume, probeGridCoords, PROBE_IRRADIANCE_TEXELS);
	uint2 cornerTexelLocation = texelLocation - 1u;
	texelLocation += input.GroupThreadId.xy;

	Texture2D<float4> irradianceHistoryTx = ResourceDescriptorHeap[ddgiVolume.irradianceHistoryIdx];
	float3 prevRadiance = irradianceHistoryTx[texelLocation].rgb;

	float3   probeDirection = DecodeNormalOctahedron(((input.GroupThreadId.xy + 0.5f) / (float)PROBE_IRRADIANCE_TEXELS) * 2 - 1);
	float3x3 randomRotation = AngleAxis3x3(PassCB.randomAngle, PassCB.randomVector);

	Buffer<float4> rayBuffer = ResourceDescriptorHeap[PassCB.rayBufferIdx];
	float  weightSum = 0;
	float3 result = 0;
	uint remainingRays = ddgiVolume.raysPerProbe;
	uint offset = 0;
    while (remainingRays > 0)
	{
		uint numRays = min(CACHE_SIZE, remainingRays);
		if(input.GroupIndex < numRays)
		{
			RadianceCache[input.GroupIndex] = rayBuffer[probeIdx * ddgiVolume.maxRaysPerProbe + offset + input.GroupIndex].rgb;
			DirectionCache[input.GroupIndex] = GetRayDirection(offset + input.GroupIndex, ddgiVolume.raysPerProbe, randomRotation);
		}
		GroupMemoryBarrierWithGroupSync();

		for(uint i = 0; i < numRays; ++i)
		{
			float3 radiance = RadianceCache[i].rgb;
			float3 direction = DirectionCache[i];
			float weight = saturate(dot(probeDirection, direction));
			result += weight * radiance;
			weightSum += weight;
		}

		remainingRays -= numRays;
        offset += numRays;
	}

	const float epsilon = 1e-9f * float(ddgiVolume.raysPerProbe);
	result *= 1.0f / max(2.0f * weightSum, epsilon);

	result = pow(result, rcp(5.0f));
	const float historyBlendWeight = saturate(1.0f - PassCB.historyBlendWeight);
	result = lerp(prevRadiance, result, historyBlendWeight);

	RWTexture2D<float4> irradianceTx = ResourceDescriptorHeap[PassCB.irradianceIdx];
	irradianceTx[texelLocation] = float4(result, 1);

	GroupMemoryBarrierWithGroupSync();

	for (uint index = input.GroupIndex; index < BORDER_TEXELS; index += PROBE_IRRADIANCE_TEXELS * PROBE_IRRADIANCE_TEXELS)
	{
		uint2 sourceIndex = cornerTexelLocation + BORDER_OFFSETS[index].xy;
		uint2 targetIndex = cornerTexelLocation + BORDER_OFFSETS[index].zw;
		irradianceTx[targetIndex] = irradianceTx[sourceIndex];
	}
}

