#ifndef _PATHTRACING_
#define _PATHTRACING_

#include "RayTracingUtil.hlsli"
#include "../Tonemapping.hlsli"

#define MIN_BOUNCES 3
#define RIS_CANDIDATES_LIGHTS 8

struct Reservoir
{
    uint LightSample;
    uint M;
    float TotalWeight;
    float SampleTargetPdf;
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

    if (lightCount <= 0)
        return false;

    Reservoir reservoir;
    reservoir.TotalWeight = 0.0f;
    reservoir.M = RIS_CANDIDATES_LIGHTS;

    for (int i = 0; i < reservoir.M; ++i)
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
        float risWeight = targetPdf / sourcePdf;
        reservoir.TotalWeight += risWeight;

        if (NextRand(seed) < (risWeight / reservoir.TotalWeight))
        {
            reservoir.LightSample = candidate;
            reservoir.SampleTargetPdf = targetPdf;
        }
    }

    if (reservoir.TotalWeight == 0.0f)
        return false;

    lightIndex = reservoir.LightSample;
    sampleWeight = (reservoir.TotalWeight / reservoir.M) / reservoir.SampleTargetPdf;
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