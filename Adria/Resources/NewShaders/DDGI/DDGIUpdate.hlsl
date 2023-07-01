#include "DDGICommon.hlsli"


#define CACHE_SIZE PROBE_IRRADIANCE_TEXELS * PROBE_IRRADIANCE_TEXELS
#define DDGI_PROBE_GAMMA 5.0f

groupshared float3 radianceCache[CACHE_SIZE];
groupshared float3 directionCache[CACHE_SIZE];

struct DDGIPassConstants
{
	float3 randomVector;
	float  randomAngle;
	float  historyBlendWeight;
	uint   rayBufferIdx;
};
ConstantBuffer<DDGIPassConstants> PassCB : register(b1);
ConstantBuffer<DDGIVolume> DDGIVolumeCB	 : register(b2);

[numthreads(PROBE_IRRADIANCE_TEXELS, PROBE_IRRADIANCE_TEXELS, 1)]
void DDGI_UpdateIrradiance(uint3 groupThreadId : SV_GroupThreadID, uint groupId : SV_GroupID, uint groupIndex : SV_GroupIndex)
{
	uint probeIdx = groupId;
	uint3 probeCoordinates = GetProbeGridCoord(DDGIVolumeCB, probeIdx);

	float3 result = 0.0f;
	float totalWeight = 0.0f;

	uint remainingRays = DDGIVolumeCB.raysPerProbe;
	uint offset = 0;

	while (remainingRays > 0)
	{
		uint numRays = min(CACHE_SIZE, remainingRays);
		if (groupIndex < numRays)
		{
			//radianceCache[groupIndex] = rayBuffer[probeIdx * volume.maxRaysPerProbe + rayIndex + groupIndex].rgb;
			//directionCache[groupIndex] = DDGIGetRayDirection(rayIndex + groupIndex, volume.NumRaysPerProbe, randomRotation);
		}
		GroupMemoryBarrierWithGroupSync();

		for (uint i = 0; i < numRays; ++i)
		{
			float3 radiance = radianceCache[i].rgb;
			float3 direction = directionCache[i];
			float weight = 0.0f; // saturate(dot(probeDirection, direction));
			sum += weight * radiance;
			totalWeight += weight;
		}
		GroupMemoryBarrierWithGroupSync();

		remainingRays -= numRays;
		offset += numRays;
	}

	
}