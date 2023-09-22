#include "DDGICommon.hlsli"
#include "../Common.hlsli"

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
	uint   irradianceIdx;
	uint   distanceIdx;
};
ConstantBuffer<DDGIPassConstants> PassCB : register(b1);
ConstantBuffer<DDGIVolume> DDGIVolumeCB	 : register(b2);

#define CACHE_SIZE PROBE_IRRADIANCE_TEXELS * PROBE_IRRADIANCE_TEXELS
groupshared float3 RadianceCache[CACHE_SIZE];
groupshared float3 DirectionCache[CACHE_SIZE];

struct CS_INPUT
{
    uint3 GroupId : SV_GroupID;
    uint3 GroupThreadId : SV_GroupThreadID;
    uint3 DispatchThreadId : SV_DispatchThreadID;
    uint  GroupIndex : SV_GroupIndex;
};

[numthreads(PROBE_IRRADIANCE_TEXELS, PROBE_IRRADIANCE_TEXELS, 1)]
void DDGI_UpdateIrradiance(CS_INPUT input)
{
	uint  probeIdx = input.GroupId.x;
	uint3 probeGridCoords = GetProbeGridCoord(DDGIVolumeCB, probeIdx);
	uint2 texelLocation = GetProbeTexel(DDGIVolumeCB, probeGridCoords, PROBE_IRRADIANCE_TEXELS);
	uint2 cornerTexelLocation = texelLocation - 1u;
	texelLocation += input.GroupThreadId.xy;

	Texture2D<float4> irradianceHistoryTx = ResourceDescriptorHeap[DDGIVolumeCB.irradianceHistoryIdx];
	float3 prevRadiance = irradianceHistoryTx[texelLocation].rgb;

	float3   probeDirection = DecodeNormalOctahedron(((input.GroupThreadId.xy + 0.5f) / (float)PROBE_IRRADIANCE_TEXELS) * 2 - 1);
	float3x3 randomRotation = AngleAxis3x3(PassCB.randomAngle, PassCB.randomVector);

	float weightSum = 0;
	float3 sum = 0;

	uint remainingRays = DDGIVolumeCB.raysPerProbe;
    while (remainingRays > 0)
	{
		uint numRays = min(CACHE_SIZE, remainingRays);
		//Fill cache
		GroupMemoryBarrierWithGroupSync();
		//todo
	}

	//todo
	GroupMemoryBarrierWithGroupSync();
}

[numthreads(1, 1, 1)]
void DDGI_UpdateIrradianceBorder(CS_INPUT input)
{
//todo
}

[numthreads(PROBE_DISTANCE_TEXELS, PROBE_DISTANCE_TEXELS, 1)]
void DDGI_UpdateDistance(CS_INPUT input)
{
//todo
}

[numthreads(1, 1, 1)]
void DDGI_UpdateDistanceBorder(CS_INPUT input)
{
//todo
}