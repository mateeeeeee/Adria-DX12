#include "DDGICommon.hlsli"
#include "../Common.hlsli"

struct DDGIPassConstants
{
	float3 randomVector;
	float  randomAngle;
	float  historyBlendWeight;
	uint   rayBufferIdx;
	uint   distanceIdx;
};
ConstantBuffer<DDGIPassConstants> PassCB : register(b1);
ConstantBuffer<DDGIVolume> DDGIVolumeCB	 : register(b2);

#define CACHE_SIZE PROBE_DISTANCE_TEXELS * PROBE_DISTANCE_TEXELS
groupshared float  DepthCache[CACHE_SIZE];
groupshared float3 DirectionCache[CACHE_SIZE];

struct CS_INPUT
{
    uint3 GroupId : SV_GroupID;
    uint3 GroupThreadId : SV_GroupThreadID;
    uint3 DispatchThreadId : SV_DispatchThreadID;
    uint  GroupIndex : SV_GroupIndex;
};

static const uint BORDER_TEXELS = PROBE_DISTANCE_TEXELS * 4 + 4;
static const uint4 BORDER_OFFSETS[BORDER_TEXELS] = 
{
	uint4(14, 14, 0, 0), 
	uint4(14, 1, 1, 0), uint4(13, 1, 2, 0), uint4(12, 1, 3, 0), uint4(11, 1, 4, 0), uint4(10, 1, 5, 0), uint4(9, 1, 6, 0), uint4(8, 1, 7, 0), 
	uint4(7, 1, 8, 0), uint4(6, 1, 9, 0), uint4(5, 1, 10, 0), uint4(4, 1, 11, 0), uint4(3, 1, 12, 0), uint4(2, 1, 13, 0), uint4(1, 1, 14, 0),
	uint4(1, 14, 15, 0),
	uint4(1, 14, 0, 1), uint4(1, 13, 0, 2), uint4(1, 12, 0, 3), uint4(1, 11, 0, 4), uint4(1, 10, 0, 5), uint4(1, 9, 0, 6), uint4(1, 8, 0, 7), 
	uint4(1, 7, 0, 8), uint4(1, 6, 0, 9), uint4(1, 5, 0, 10), uint4(1, 4, 0, 11), uint4(1, 3, 0, 12), uint4(1, 2, 0, 13), uint4(1, 1, 0, 14),

	uint4(14, 1, 0, 15), 
	uint4(14, 14, 15, 1), uint4(14, 13, 15, 2), uint4(14, 12, 15, 3), uint4(14, 11, 15, 4), uint4(14, 10, 15, 5), uint4(14, 9, 15, 6), uint4(14, 8, 15, 7), 
	uint4(14, 7, 15, 8), uint4(14, 6, 15, 9), uint4(14, 5, 15, 10), uint4(14, 4, 15, 11), uint4(14, 3, 15, 12), uint4(14, 2, 15, 13), uint4(14, 1, 15, 14),

	uint4(1, 1, 15, 15), 
	uint4(14, 14, 1, 15), uint4(13, 14, 2, 15), uint4(12, 14, 3, 15), uint4(11, 14, 4, 15), uint4(10, 14, 5, 15), uint4(9, 14, 6, 15), uint4(8, 14, 7, 15), 
	uint4(7, 14, 8, 15), uint4(6, 14, 9, 15), uint4(5, 14, 10, 15), uint4(4, 14, 11, 15), uint4(3, 14, 12, 15), uint4(2, 14, 13, 15), uint4(1, 14, 14, 15),
};

[numthreads(PROBE_DISTANCE_TEXELS, PROBE_DISTANCE_TEXELS, 1)]
void DDGI_UpdateDistance(CS_INPUT input)
{
	uint  probeIdx = input.GroupId.x;
	uint3 probeGridCoords = GetProbeGridCoord(DDGIVolumeCB, probeIdx);
	uint2 texelLocation = GetProbeTexel(DDGIVolumeCB, probeGridCoords, PROBE_DISTANCE_TEXELS);
	uint2 cornerTexelLocation = texelLocation - 1u;
	texelLocation += input.GroupThreadId.xy;

	Texture2D<float2> distanceHistoryTx = ResourceDescriptorHeap[DDGIVolumeCB.distanceHistoryIdx];
	float2 prevDistance = distanceHistoryTx[texelLocation];

	float3   probeDirection = DecodeNormalOctahedron(((input.GroupThreadId.xy + 0.5f) / (float)PROBE_DISTANCE_TEXELS) * 2 - 1);
	float3x3 randomRotation = AngleAxis3x3(PassCB.randomAngle, PassCB.randomVector);

	Buffer<float4> rayBuffer = ResourceDescriptorHeap[PassCB.rayBufferIdx];
	float  weightSum = 0;
	float2 result = 0;
	uint remainingRays = DDGIVolumeCB.raysPerProbe;
	uint offset = 0;
    while (remainingRays > 0)
	{
		uint numRays = min(CACHE_SIZE, remainingRays);
		if(input.GroupIndex < numRays)
		{
			DepthCache[input.GroupIndex] = rayBuffer[probeIdx * DDGIVolumeCB.maxRaysPerProbe + offset + input.GroupIndex].a;
			DirectionCache[input.GroupIndex] = GetRayDirection(offset + input.GroupIndex, DDGIVolumeCB.raysPerProbe, randomRotation);
		}
		GroupMemoryBarrierWithGroupSync();

		for(uint i = 0; i < numRays; ++i)
		{
			float depth = DepthCache[i];
			float3 direction = DirectionCache[i];
			float weight = saturate(dot(probeDirection, direction));
			weight = pow(weight, 64);
			if(weight > MIN_WEIGHT)
			{
				weightSum += weight;
				result += float2(abs(depth), pow2(depth)) * weight;
			}
		}
		remainingRays -= numRays;
        offset += numRays;
	}

	if(weightSum > MIN_WEIGHT)
	{
		result /= weightSum;
	}

	const float historyBlendWeight = saturate(1.0f - PassCB.historyBlendWeight);
	result = lerp(prevDistance, result, historyBlendWeight);

	RWTexture2D<float2> distanceTx = ResourceDescriptorHeap[PassCB.distanceIdx];
	distanceTx[texelLocation] = result;

	GroupMemoryBarrierWithGroupSync();

	for (uint index = input.GroupIndex; index < BORDER_TEXELS; index += PROBE_DISTANCE_TEXELS * PROBE_DISTANCE_TEXELS)
	{
		uint2 sourceIndex = cornerTexelLocation + BORDER_OFFSETS[index].xy;
		uint2 targetIndex = cornerTexelLocation + BORDER_OFFSETS[index].zw;
		distanceTx[targetIndex] = distanceTx[sourceIndex];
	}
}

