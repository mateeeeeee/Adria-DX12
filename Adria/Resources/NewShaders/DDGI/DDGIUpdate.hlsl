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

}

[numthreads(1, 1, 1)]
void DDGI_UpdateBordersIrradiance(uint3 groupThreadId : SV_GroupThreadID, uint groupId : SV_GroupID, uint groupIndex : SV_GroupIndex)
{

}

[numthreads(PROBE_DISTANCE_TEXELS, PROBE_DISTANCE_TEXELS, 1)]
void DDGI_UpdateDistance(uint3 groupThreadId : SV_GroupThreadID, uint groupId : SV_GroupID, uint groupIndex : SV_GroupIndex)
{

}

[numthreads(1, 1, 1)]
void DDGI_UpdateBordersDistance(uint3 groupThreadId : SV_GroupThreadID, uint groupId : SV_GroupID, uint groupIndex : SV_GroupIndex)
{

}