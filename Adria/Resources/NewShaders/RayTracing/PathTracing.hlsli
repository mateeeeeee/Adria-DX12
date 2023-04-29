#ifndef PATHTRACING_INCLUDED
#define PATHTRACING_INCLUDED

#include "RayTracingUtil.hlsli"
#include "../Scene.hlsli"
#include "../BRDF.hlsli"
#include "../Lighting.hlsli"
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

struct MaterialProperties
{
    float3 baseColor;
    float3 normalTS;
    float metallic;
    float3 emissive;
    float roughness;
    float opacity;
    float specular;
};

MaterialProperties GetMaterialProperties(Material material, float2 UV, int mipLevel)
{
    MaterialProperties properties = (MaterialProperties) 0;
    float4 baseColor = float4(material.baseColorFactor, 1.0f);
    if (material.diffuseIdx >= 0)
    {
        baseColor *= SampleBindlessLevel2D(material.diffuseIdx, LinearWrapSampler, UV, mipLevel);
    }
    properties.baseColor = baseColor.rgb;
    properties.opacity = baseColor.a;

    properties.metallic = material.metallicFactor;
    properties.roughness = material.roughnessFactor;
    if (material.roughnessMetallicIdx >= 0)
    {
        float4 roughnessMetalnessSample = SampleBindlessLevel2D(material.roughnessMetallicIdx, LinearWrapSampler, UV, mipLevel);
        properties.metallic *= roughnessMetalnessSample.b;
        properties.roughness *= roughnessMetalnessSample.g;
    }
    properties.emissive = material.emissiveFactor.rrr;
    if (material.emissiveIdx >= 0)
    {
        properties.emissive *= SampleBindlessLevel2D(material.emissiveIdx, LinearWrapSampler, UV, mipLevel).rgb;
    }
    properties.specular = 0.5f;

    properties.normalTS = float3(0.5f, 0.5f, 1.0f);
    if (material.normalIdx >= 0)
    {
        properties.normalTS = SampleBindlessLevel2D(material.normalIdx, LinearWrapSampler, UV, mipLevel).rgb;
    }
    return properties;
}
BrdfData GetBrdfData(MaterialProperties material)
{
    BrdfData data;
    data.Diffuse = ComputeDiffuseColor(material.baseColor, material.metallic);
    data.Specular = ComputeF0(material.specular, material.baseColor, material.metallic);
    data.Roughness = material.roughness;
    return data;
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