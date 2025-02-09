#include "Random.hlsli"
#include "Tonemapping.hlsli"
#include "CommonResources.hlsli"

#define RIS_CANDIDATES_LIGHTS 8

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
        reservoir.lightData = lightIndex;
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
    float sampleM = 1.0f,               // In its most basic form, should be newReservoir.M
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
bool ReSTIR_DI_CombineDIReservoirs(
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