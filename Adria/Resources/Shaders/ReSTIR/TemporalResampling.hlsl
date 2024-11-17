#include "CommonResources.hlsli"
#include "Packing.hlsli"
#include "Constants.hlsli"
#include "Random.hlsli"
#include "RayTracing/RayTracingUtil.hlsli"


struct TemporalResamplingConstants
{

};

ConstantBuffer<TemporalResamplingConstants> TemporalResamplingCB : register(b1);

[numthreads(16, 16, 1)]
void TemporalResamplingCS( uint3 dispatchThreadID : SV_DispatchThreadID )
{
}
