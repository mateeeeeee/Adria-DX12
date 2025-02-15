#ifndef _PATHTRACING_
#define _PATHTRACING_
#include "RayTracingUtil.hlsli"
#include "Tonemapping.hlsli"

#define MIN_BOUNCES 3
#define RIS_CANDIDATES_LIGHTS 8

struct Reservoir
{
    bool UpdateReservoir(uint X, float w, float pdf, float random)
    {
        totalWeight += w;
        M += 1;
 
        if (random < w / totalWeight)
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
    uint  M;                
};

void SampleSourceLight(in int lightCount, inout RNG rng, out int lightIndex, out float sourcePdf)
{
    lightIndex = min(int(RNG_GetNext(rng) * lightCount), lightCount - 1);
    sourcePdf = 1.0f / lightCount;
}
bool SampleLightRIS(inout RNG rng, float3 position, float3 N, out int lightIndex, out float sampleWeight)
{
    uint M = min(RIS_CANDIDATES_LIGHTS, FrameCB.lightCount);
    lightIndex = -1;
    sampleWeight = 0.0f;

    Reservoir reservoir = (Reservoir)0;
    for (int i = 0; i < M; ++i)
    {
        uint lightIndex = 0;
        float sourcePdf = 1.0f;
        SampleSourceLight(FrameCB.lightCount, rng, lightIndex, sourcePdf);

        LightInfo lightInfo = LoadLightInfo(lightIndex); 
        float3 positionDifference = lightInfo.position.xyz - position;
        float distance = length(positionDifference);
        float3 L = positionDifference / distance;
        if (lightInfo.type == DIRECTIONAL_LIGHT)
        {
            L = -normalize(lightInfo.direction.xyz);
        }
        if (dot(N, L) < 0.0f)
        {
            continue;
        }
        float targetPdf = Luminance(DoAttenuation(distance, lightInfo.range) * lightInfo.color.rgb);
        if (lightInfo.type == DIRECTIONAL_LIGHT)
        {
            targetPdf = Luminance(lightInfo.color.rgb);
        }
        float risWeight = targetPdf / sourcePdf;
        
        reservoir.UpdateReservoir(lightIndex, risWeight, targetPdf, RNG_GetNext(rng));
    }

    if (reservoir.totalWeight == 0.0f) return false;

    lightIndex = reservoir.lightIndex;
    sampleWeight = reservoir.GetSampleWeight();
    return true;
}

float ProbabilityToSampleDiffuse(float3 diffuse, float3 specular)
{
    float lumDiffuse = Luminance(diffuse);
    float lumSpecular = Luminance(specular);
    return lumDiffuse / max(lumDiffuse + lumSpecular, 0.0001);
}


float3 SampleGGX(float2 randVal, float roughness, float3 N)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float cosThetaH = sqrt(max(0.0f, (1.0 - randVal.x) / ((a2 - 1.0) * randVal.x + 1)));
    float sinThetaH = sqrt(max(0.0f, 1.0f - cosThetaH * cosThetaH));
    float phiH = randVal.y * M_PI * 2.0f;

    float3 v = float3(sinThetaH * cos(phiH), sinThetaH * sin(phiH), cosThetaH);
    return TangentToWorld(v, N);
}

#endif