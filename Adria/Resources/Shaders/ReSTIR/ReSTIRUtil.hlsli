#include "Random.hlsli"
#include "Tonemapping.hlsli"
#include "CommonResources.hlsli"

#define RIS_CANDIDATES_LIGHTS 8


struct ReSTIR_DI_ReservoirSample
{
	float3 samplePosition;
	float3 sampleNormal;
	float3 sampleRadiance;

	void Reset()
	{
		samplePosition = 0;
		sampleNormal = 0;
		sampleRadiance = 0;
	}
};

struct ReSTIR_DI_Reservoir
{
	ReSTIR_DI_ReservoirSample sample;
	float targetFunction;
	float numSamples;
	float weightSum;

    void Reset()
    {
        weightSum = 0;
        targetFunction = 0;
        numSamples = 0;
        sample.Reset();
    }

    void InternalResample(ReSTIR_DI_Reservoir r, float newTargetFunction, float risWeight, float rand)
    {
        numSamples += r.numSamples;
        weightSum += risWeight;

        if (rand * weightSum < risWeight)
        {
            sample = r.sample;
            targetFunction = newTargetFunction;
        }
    }

    void Combine(ReSTIR_DI_Reservoir r, float newTargetFunction, float rand)
    {
        float ucw = r.weightSum;
        float misScalingFactor = r.numSamples;
        float risWeight = newTargetFunction * ucw * misScalingFactor;
        InternalResample(r, newTargetFunction, risWeight, rand);
    }

    void End()
    {
        float denom = targetFunction * numSamples;
        m_WeightSum = denom <= 0.0f ? 0.0f : m_WeightSum / denom;
    }
};

