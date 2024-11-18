#include "Random.hlsli"
#include "Tonemapping.hlsli"
#include "CommonResources.hlsli"

#define RIS_CANDIDATES_LIGHTS 8

struct Reservoir
{
    bool UpdateReservoir(uint X, float w, float pdf, inout uint randSeed)
    {
        totalWeight += w;
        M += 1;
 
        if (NextRand(randSeed) < w / totalWeight)
        {
            lightIndex = X;
            lightWeight = w;
            sampleTargetPdf = pdf;
            return true;
        }
 
        return false;
    }

    float GetSampleWeight()
    {
        return (totalWeight / M) / sampleTargetPdf;
    }

    uint  lightIndex;       
    float lightWeight;      
    float totalWeight; 
    float sampleTargetPdf;     
    uint  M;                // number of lights processed for this reservoir
};



void SampleSourceLight(in uint lightCount, inout uint seed, out uint lightIndex, out float sourcePdf)
{
    lightIndex = min(uint(NextRand(seed) * lightCount), lightCount - 1);
    sourcePdf = 1.0f / lightCount;
}
bool SampleLightRIS(inout uint seed, float3 position, float3 N, out int lightIndex, out float sampleWeight)
{
    StructuredBuffer<Light> lights = ResourceDescriptorHeap[FrameCB.lightsIdx];
    uint lightCount, _unused;
    lights.GetDimensions(lightCount, _unused);

    uint M = min(RIS_CANDIDATES_LIGHTS, lightCount);
    lightIndex = -1;
    sampleWeight = 0.0f;

    Reservoir reservoir = (Reservoir)0;
    for (int i = 0; i < M; ++i)
    {
        uint candidate = 0;
        float sourcePdf = 1.0f;
        SampleSourceLight(lightCount, seed, candidate, sourcePdf);

        Light light = lights[candidate];
        float3 positionDifference = light.position.xyz - position;
        float distance = length(positionDifference);
        float3 L = positionDifference / distance;
        if (light.type == DIRECTIONAL_LIGHT)
        {
            L = -normalize(light.direction.xyz);
        }
        if (dot(N, L) < 0.0f)
        {
            continue;
        }
        float targetPdf = Luminance(DoAttenuation(distance, light.range) * light.color.rgb);
        if (light.type == DIRECTIONAL_LIGHT)
        {
            targetPdf = Luminance(light.color.rgb);
        }
        float risWeight = targetPdf / sourcePdf;
        reservoir.UpdateReservoir(candidate, risWeight, targetPdf, seed);
    }

    if (reservoir.totalWeight == 0.0f) return false;

    lightIndex = reservoir.lightIndex;
    sampleWeight = reservoir.GetSampleWeight();
    return true;
}