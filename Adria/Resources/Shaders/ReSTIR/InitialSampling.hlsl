#include "CommonResources.hlsli"
#include "Packing.hlsli"
#include "Constants.hlsli"
#include "Random.hlsli"

struct ReSTIRConstants
{
    uint depthIdx;
    uint normalIdx;
    uint irradianceHistoryIdx;
    uint outputIrradianceIdx;
    uint outputRayDirectionIdx;
};
ConstantBuffer<ReSTIRConstants> PassCB : register(b1);

[numthreads(16, 16, 1)]
void InitialSamplingCS( uint3 dispatchThreadID : SV_DispatchThreadID )
{
    Texture2D<float> depthTexture = ResourceDescriptorHeap[PassCB.depthIdx];
    Texture2D<float3> normalTexture = ResourceDescriptorHeap[PassCB.normalIdx];
    RWTexture2D<float4> outputRadiance = ResourceDescriptorHeap[PassCB.outputIrradianceIdx];
    RWTexture2D<uint> outputRayDirection = ResourceDescriptorHeap[PassCB.outputRayDirectionIdx];
    
    float depth = depthTexture[dispatchThreadID.xy].r;
    float3 N = normalTexture[dispatchThreadID.xy];

    if (depth == 0.0)
    {
        outputRadiance[dispatchThreadID.xy] = 0.xxxx;
        outputRayDirection[dispatchThreadID.xy] = 0;
        return;
    }
    float3 worldPos = GetWorldPosition(FullScreenPosition(dispatchThreadID.xy), depth);

    float2 randFloat2 = float2(0.0f, 0.0f);
    float3 direction = float3(0.0f, 0.0f, 0.0f); // uniform hemisphere sampling
    float pdf = 1.0 / (2.0 * M_PI);

    RayDesc ray;
    ray.Origin = worldPos + N * 0.01;
    ray.Direction = direction;
    ray.TMin = 0.00001;
    ray.TMax = 1000.0;
}