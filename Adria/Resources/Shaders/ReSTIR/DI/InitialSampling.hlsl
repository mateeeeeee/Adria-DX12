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
    Texture2D<float> depthTexture   = ResourceDescriptorHeap[IntialSamplingCB.depthIdx];
    Texture2D<float4> normalRT = ResourceDescriptorHeap[IntialSamplingCB.normalIdx];
    Texture2D<float4> albedoRT = ResourceDescriptorHeap[IntialSamplingCB.albedoIdx];
    RWStructuredBuffer<ReSTIR_DI_Reservoir> reservoirBuffer = ResourceDescriptorHeap[IntialSamplingCB.reservoirBufferIdx];

    float3 viewNormal;
	float metallic;
	uint  shadingExtension;
	float4 normalRTData = normalRT[DTid.xy];
	DecodeGBufferNormalRT(normalRTData, viewNormal, metallic, shadingExtension);

    float depth = depthRT[DTid.xy].r;
    if (depth == 0.0f) 
    {
        ReSTIR_DI_Reservoir emptyReservoir = (ReSTIR_DI_Reservoir)0;
        emptyReservoir.Reset();
        reservoirBuffer[DTid.xy] = emptyReservoir;
        return;
    }
    
    viewNormal = 2.0f * viewNormal - 1.0f;
    N = normalize(mul(viewNormal, (float3x3)FrameCB.view));
    
    float3 worldPos = GetWorldPosition(FullScreenPosition(DTid.xy), depth);
    float3 V = normalize(FrameCB.cameraPosition - worldPos);
    
    uint randSeed = InitRand(DTid.x + DTid.y * 16, 0, 16);
    float3 worldPos = GetWorldPosition(FullScreenPosition(DTid.xy), depth);
    
    ReSTIR_DI_LightSample lightSample = ReSTIR_DI_EmptyLightSample();
    ReSTIR_DI_Reservoir finalReservoir = ReSTIR_DI_SampleLightsForSurface(randSeed, lightSample);
 
    reservoirBuffer[pixelIndex] = finalReservoir;
}
