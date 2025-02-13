#include "Packing.hlsli"
#include "Constants.hlsli"
#include "ReSTIR_DI_Util.hlsli"
#include "RayTracing/RayTracingUtil.hlsli"


struct IntialSamplingConstants
{
    uint depthIdx;
    uint normalIdx;
    uint albedoIdx;
    uint reservoirBufferIdx;
};
ConstantBuffer<IntialSamplingConstants> IntialSamplingCB : register(b1);

[numthreads(16, 16, 1)]
void InitialSamplingCS( uint3 DTid : SV_DispatchThreadID )
{
    Surface surface = GetSurface(DTid.xy, IntialSamplingCB.albedoIdx, IntialSamplingCB.normalIdx, IntialSamplingCB.depthIdx);
    if (surface.depth == 0.0f) 
    {
        ReSTIR_DI_Reservoir emptyReservoir = (ReSTIR_DI_Reservoir)0;
        emptyReservoir.Reset();
        ReSTIR_DI_StoreReservoir(emptyReservoir, DTid.xy, IntialSamplingCB.reservoirBufferIdx);
        return;
    }
    RNG rng = RNG_Initialize(DTid.x + DTid.y * 16, 0, 16);
    
    ReSTIR_DI_LightSample lightSample = ReSTIR_DI_EmptyLightSample();
    ReSTIR_DI_Reservoir finalReservoir = ReSTIR_DI_SampleLightsForSurface(rng, surface, lightSample);
    //check visibility of a light sample
    ReSTIR_DI_StoreReservoir(finalReservoir, DTid.xy, IntialSamplingCB.reservoirBufferIdx);
}
