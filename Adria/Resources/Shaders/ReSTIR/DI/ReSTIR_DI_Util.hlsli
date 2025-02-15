#include "ReSTIR/ReSTIR_Util.hlsli"

//reference: https://github.com/NVIDIA-RTX/RTXDI-Library/blob/main/Include/Rtxdi/DI/Reservoir.hlsli

// This structure represents a single light reservoir that stores the weights, the sample ref, sample count (M)
struct ReSTIR_DI_Reservoir
{
	uint lightIndex;
	uint uvData; // Sample UV encoded in 16-bit fixed point format
	float weightSum; // Overloaded: represents RIS weight sum during streaming, then reservoir weight (inverse PDF) after FinalizeResampling
	float targetPdf; // Target PDF of the selected sample
	float M; // Number of samples considered for this reservoir (pairwise MIS makes this a float)
};


ReSTIR_DI_Reservoir ReSTIR_DI_EmptyDIReservoir()
{
    ReSTIR_DI_Reservoir r = (ReSTIR_DI_Reservoir)0;
    return r;
}

uint ReSTIR_DI_GetLightIndex(const ReSTIR_DI_Reservoir reservoir)
{
    return reservoir.lightIndex;
}

float2 ReSTIR_DI_GetSampleUV(const ReSTIR_DI_Reservoir reservoir)
{
    return float2(reservoir.uvData & 0xffff, reservoir.uvData >> 16) / float(0xffff);
}

float ReSTIR_DI_GetInvPdf(const ReSTIR_DI_Reservoir reservoir)
{
    return reservoir.weightSum;
}

// Adds a new, non-reservoir light sample into the reservoir, returns true if this sample was selected.
// Algorithm (3) from the ReSTIR paper, Streaming RIS using weighted reservoir sampling.
bool ReSTIR_DI_StreamSample(
    inout ReSTIR_DI_Reservoir reservoir,
    uint lightIndex,
    float2 uv,
    float random,
    float targetPdf,
    float invSourcePdf)
{
    // What's the current weight
    float risWeight = targetPdf * invSourcePdf;

    // Add one sample to the counter
    reservoir.M += 1;

    // Update the weight sum
    reservoir.weightSum += risWeight;

    // Decide if we will randomly pick this sample
    bool selectSample = (random * reservoir.weightSum < risWeight);

    // If we did select this sample, update the relevant data.
    // New samples don't have visibility or age information, we can skip that.
    if (selectSample)
    {
        reservoir.lightIndex = lightIndex;
        reservoir.uvData = uint(saturate(uv.x) * 0xffff) | (uint(saturate(uv.y) * 0xffff) << 16);
        reservoir.targetPdf = targetPdf;
    }
    return selectSample;
}

// Adds `newReservoir` into `reservoir`, returns true if the new reservoir's sample was selected.
// This is a very general form, allowing input parameters to specfiy normalization and targetPdf
// rather than computing them from `newReservoir`.  Named "internal" since these parameters take
// different meanings
bool ReSTIR_DI_InternalSimpleResample(
    inout ReSTIR_DI_Reservoir reservoir,
    const ReSTIR_DI_Reservoir newReservoir,
    float random,
    float targetPdf = 1.0f,             // Usually closely related to the sample normalization, 
    float sampleNormalization = 1.0f,   // typically off by some multiplicative factor 
    float sampleM = 1.0f                // In its most basic form, should be newReservoir.M
)
{
    // What's the current weight (times any prior-step RIS normalization factor)
    float risWeight = targetPdf * sampleNormalization;

    // Our *effective* candidate pool is the sum of our candidates plus those of our neighbors
    reservoir.M += sampleM;

    // Update the weight sum
    reservoir.weightSum += risWeight;

    // Decide if we will randomly pick this sample
    bool selectSample = (random * reservoir.weightSum < risWeight);

    // If we did select this sample, update the relevant data
    if (selectSample)
    {
        reservoir.lightIndex = newReservoir.lightIndex;
        reservoir.uvData = newReservoir.uvData;
        reservoir.targetPdf = targetPdf;
    }
    return selectSample;
}

// Adds `newReservoir` into `reservoir`, returns true if the new reservoir's sample was selected.
// Algorithm (4) from the ReSTIR paper, Combining the streams of multiple reservoirs.
// Normalization - Equation (6) - is postponed until all reservoirs are combined.
bool ReSTIR_DI_CombineReservoirs(
    inout ReSTIR_DI_Reservoir reservoir,
    const ReSTIR_DI_Reservoir newReservoir,
    float random,
    float targetPdf)
{
    return ReSTIR_DI_InternalSimpleResample(
        reservoir,
        newReservoir,
        random,
        targetPdf,
        newReservoir.weightSum * newReservoir.M,
        newReservoir.M
    );
}

// Performs normalization of the reservoir after streaming. Equation (6) from the ReSTIR paper.
void ReSTIR_DI_FinalizeResampling(
    inout ReSTIR_DI_Reservoir reservoir,
    float normalizationNumerator,
    float normalizationDenominator)
{
    float denominator = reservoir.targetPdf * normalizationDenominator;
    reservoir.weightSum = (denominator == 0.0) ? 0.0 : (reservoir.weightSum * normalizationNumerator) / denominator;
}

void ReSTIR_DI_RandomlySelectLight(
    inout RNG rng,
    uint lightCount,
    out LightInfo lightInfo,
    out uint lightIndex,
    out float invSourcePdf)
{
    lightIndex = 0;
    invSourcePdf = 0.0f;
    float rnd = RNG_GetNext(rng);
    invSourcePdf = float(lightCount);
    lightIndex = 1 + min(uint(floor(rnd * lightCount)), lightCount - 1);
    lightInfo = LoadLightInfo(lightIndex);
}

bool ReSTIR_DI_StreamLocalLightAtUVIntoReservoir(
    inout RNG rng,
    Surface surface,
    uint lightIndex,
    float2 uv,
    float invSourcePdf,
    LightInfo lightInfo,
    inout ReSTIR_DI_Reservoir state,
    inout LightSample selectedSample)
{
    LightSample candidateSample = ReSTIR_SampleLight(lightInfo, surface, uv);
    float blendedSourcePdf = 0.5f; //LightBrdfMisWeight();
    float targetPdf = ReSTIR_GetLightSampleTargetPdfForSurface(candidateSample, surface);
    float risRnd = RNG_GetNext(rng);
    if (blendedSourcePdf == 0)
    {
        return false;
    }
    bool selected = ReSTIR_DI_StreamSample(state, lightIndex, uv, risRnd, targetPdf, 1.0 / blendedSourcePdf);
    if (selected) 
    {
        selectedSample = candidateSample;
    }
    return true;
}

void ReSTIR_DI_StreamInfiniteLightAtUVIntoReservoir(
    inout RNG rng,
    LightInfo lightInfo,
    Surface surface,
    uint lightIndex,
    float2 uv,
    float invSourcePdf,
    inout ReSTIR_DI_Reservoir state,
    inout LightSample selectedSample)
{
    LightSample candidateSample = ReSTIR_SampleLight(lightInfo, surface, uv);
    float targetPdf = ReSTIR_GetLightSampleTargetPdfForSurface(candidateSample, surface);
    float risRnd = RNG_GetNext(rng);
    bool selected = ReSTIR_DI_StreamSample(state, lightIndex, uv, risRnd, targetPdf, invSourcePdf);
    if (selected)
    {
        selectedSample = candidateSample;
    }
}

ReSTIR_DI_Reservoir ReSTIR_DI_SampleLocalLights(inout RNG rng, Surface surface, out LightSample lightSample)
{
    if (FrameCB.lightCount <= 1) //we assume that the only directional light is at 0, and all others are locals
    {
        return ReSTIR_DI_EmptyDIReservoir();
    }
    const uint localLightCount = FrameCB.lightCount - 1;

    ReSTIR_DI_Reservoir state = ReSTIR_DI_EmptyDIReservoir();
    const uint localLightSamples = 4;
    for (uint i = 0; i < localLightSamples; i++)
    {
        uint lightIndex;
        LightInfo lightInfo;
        float invSourcePdf;
        ReSTIR_DI_RandomlySelectLight(rng, localLightCount, lightInfo, lightIndex, invSourcePdf);
        float2 uv = ReSTIR_RandomlySelectLocalLightUV(rng);
        ReSTIR_DI_StreamLocalLightAtUVIntoReservoir(rng, surface, lightIndex, uv, invSourcePdf, lightInfo, state, lightSample);
    }

    ReSTIR_DI_FinalizeResampling(state, 1.0, 1); // sampleParams.numMisSamples
    state.M = 1;
    return state;
}
ReSTIR_DI_Reservoir ReSTIR_DI_SampleInfiniteLights(inout RNG rng, Surface surface, out LightSample lightSample)
{
    ReSTIR_DI_Reservoir state = ReSTIR_DI_EmptyDIReservoir();
    lightSample = EmptyLightSample();

    if (FrameCB.lightCount == 0)
        return state;

    const uint infiniteLightSamples = 1;
    for (uint i = 0; i < infiniteLightSamples; i++)
    {
        float invSourcePdf;
        uint lightIndex;
        LightInfo lightInfo;

        ReSTIR_DI_RandomlySelectLight(rng, 1, lightInfo, lightIndex, invSourcePdf);
        float2 uv = ReSTIR_RandomlySelectInfiniteLightUV(rng);
        ReSTIR_DI_StreamInfiniteLightAtUVIntoReservoir(rng, lightInfo, surface, lightIndex, uv, invSourcePdf, state, lightSample);
    }

    ReSTIR_DI_FinalizeResampling(state, 1.0, state.M);
    state.M = 1;
    return state;
}
ReSTIR_DI_Reservoir ReSTIR_DI_SampleBrdf(inout RNG rng, Surface surface, out LightSample lightSample)
{
    ReSTIR_DI_Reservoir state = ReSTIR_DI_EmptyDIReservoir();
    const uint brdfLightSamples = 1;
    for (uint i = 0; i < brdfLightSamples; ++i)
    {
        float lightSourcePdf = 0;
        float3 sampleDir;
        uint lightIndex = ReSTIR_InvalidLightIndex;
        float2 randXY = float2(0, 0);
        LightSample candidateSample = EmptyLightSample();
        
        //if (GetSurfaceBrdfSample(surface, rng, sampleDir))
        //{
        //    float brdfPdf = RAB_GetSurfaceBrdfPdf(surface, sampleDir);
        //    float maxDistance = RTXDI_BrdfMaxDistanceFromPdf(sampleParams.brdfCutoff, brdfPdf);
        //    
        //    bool hitAnything = RAB_TraceRayForLocalLight(RAB_GetSurfaceWorldPos(surface), sampleDir,
        //        sampleParams.brdfRayMinT, maxDistance, lightIndex, randXY);
        //
        //    if (lightIndex != ReSTIR_InvalidLightIndex)
        //    {
        //        LightInfo lightInfo = LoadLightInfo(lightIndex);
        //        candidateSample = RAB_SamplePolymorphicLight(lightInfo, surface, randXY);
        //            
        //        if (sampleParams.brdfCutoff > 0.f)
        //        {
        //            // If Mis cutoff is used, we need to evaluate the sample and make sure it actually could have been
        //            // generated by the area sampling technique. This is due to numerical precision.
        //            float3 lightDir;
        //            float lightDistance;
        //            RAB_GetLightDirDistance(surface, candidateSample, lightDir, lightDistance);
        //
        //            float brdfPdf = RAB_GetSurfaceBrdfPdf(surface, lightDir);
        //            float maxDistance = RTXDI_BrdfMaxDistanceFromPdf(sampleParams.brdfCutoff, brdfPdf);
        //            if (lightDistance > maxDistance)
        //                lightIndex = RTXDI_InvalidLightIndex;
        //        }
        //
        //        if (lightIndex != ReSTIR_InvalidLightIndex)
        //        {
        //            lightSourcePdf = RAB_EvaluateLocalLightSourcePdf(lightIndex);
        //        }
        //    }
        //    else if (!hitAnything && (lightBufferParams.environmentLightParams.lightPresent != 0))
        //    {
        //        // sample environment light
        //        //TODO
        //    }
        //}
        //
        //if (lightSourcePdf == 0)
        //{
        //    // Did not hit a visible light
        //    continue;
        //}
        //
        //bool isEnvMapSample = lightIndex == lightBufferParams.environmentLightParams.lightIndex;
        //float targetPdf = RAB_GetLightSampleTargetPdfForSurface(candidateSample, surface);
        //float blendedSourcePdf = RTXDI_LightBrdfMisWeight(surface, candidateSample, lightSourcePdf,
        //    isEnvMapSample ? sampleParams.environmentMapMisWeight : sampleParams.localLightMisWeight, 
        //    isEnvMapSample,
        //    sampleParams);
        //float risRnd = RNG_GetNext(rng);
        //bool selected = ReSTIR_DI_StreamSample(state, lightIndex, randXY, risRnd, targetPdf, 1.0f / blendedSourcePdf);
        //if (selected) 
        //{
        //    lightSample = candidateSample;
        //}
    }
    ReSTIR_DI_FinalizeResampling(state, 1.0, 1.0);
    state.M = 1;
    return state;
}

ReSTIR_DI_Reservoir ReSTIR_DI_SampleLightsForSurface(inout RNG rng, Surface surface, out LightSample lightSample)
{
    lightSample = EmptyLightSample();

    LightSample localSample = EmptyLightSample();
    ReSTIR_DI_Reservoir localReservoir = ReSTIR_DI_SampleLocalLights(rng, surface, lightSample);

    LightSample infiniteSample = EmptyLightSample();  
    ReSTIR_DI_Reservoir infiniteReservoir = ReSTIR_DI_SampleInfiniteLights(rng, surface, lightSample);

    LightSample brdfSample = EmptyLightSample();
    ReSTIR_DI_Reservoir brdfReservoir = ReSTIR_DI_SampleBrdf(rng, surface, lightSample);

    ReSTIR_DI_Reservoir state = ReSTIR_DI_EmptyDIReservoir();
    ReSTIR_DI_CombineReservoirs(state, localReservoir, 0.5, localReservoir.targetPdf);

    bool selectInfinite = ReSTIR_DI_CombineReservoirs(state, infiniteReservoir, RNG_GetNext(rng), infiniteReservoir.targetPdf);
    bool selectBrdf = ReSTIR_DI_CombineReservoirs(state, brdfReservoir, RNG_GetNext(rng), brdfReservoir.targetPdf);
    
    ReSTIR_DI_FinalizeResampling(state, 1.0, 1.0);
    state.M = 1;

    if (selectBrdf) lightSample = brdfSample;
    else if (selectInfinite) lightSample = infiniteSample;
    else lightSample = localSample;
    return state;
}

void ReSTIR_DI_StoreReservoir(
    const ReSTIR_DI_Reservoir reservoir,
    uint2 pixelPosition, uint reservoirBufferIdx)
{
    uint flattenIndex = pixelPosition.x * FrameCB.renderResolution.x + pixelPosition.y;
    RWStructuredBuffer<ReSTIR_DI_Reservoir> reservoirBuffer = ResourceDescriptorHeap[reservoirBufferIdx];
    reservoirBuffer[flattenIndex] = reservoir;
}

ReSTIR_DI_Reservoir ReSTIR_DI_LoadReservoir(uint2 pixelPosition, uint reservoirBufferIdx)
{
    uint flattenIndex = pixelPosition.x * FrameCB.renderResolution.x + pixelPosition.y;
    StructuredBuffer<ReSTIR_DI_Reservoir> reservoirBuffer = ResourceDescriptorHeap[reservoirBufferIdx];
    return reservoirBuffer[flattenIndex];
}