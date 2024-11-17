#include "CommonResources.hlsli"
#include "Packing.hlsli"
#include "Constants.hlsli"
#include "Random.hlsli"
#include "RayTracing/RayTracingUtil.hlsli"


struct SpatialResamplingConstants
{

};

ConstantBuffer<SpatialResamplingConstants> SpatialResamplingCB : register(b1);

[numthreads(16, 16, 1)]
void SpatialResamplingCS( uint3 dispatchThreadID : SV_DispatchThreadID )
{
}
