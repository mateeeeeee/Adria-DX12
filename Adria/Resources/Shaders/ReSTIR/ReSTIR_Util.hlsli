#include "Random.hlsli"
#include "Tonemapping.hlsli"
#include "Surface.hlsli"
#include "LightInfo.hlsli"

static const uint ReSTIR_InvalidLightIndex = uint(-1);


float2 ReSTIR_RandomlySelectInfiniteLightUV(inout RNG rng)
{
    float2 uv;
    uv.x = RNG_GetNext(rng);
    uv.y = RNG_GetNext(rng);
    return uv;
}

float2 ReSTIR_RandomlySelectLocalLightUV(inout RNG rng)
{
    float2 uv;
    uv.x = RNG_GetNext(rng);
    uv.y = RNG_GetNext(rng);
    return uv;
}

// Evaluate the surface BRDF and compute the weighted reflected radiance for the given light sample
float3 ReSTIR_ShadeSurfaceWithLightSample(LightSample lightSample, Surface surface)
{
    // Ignore invalid light samples
    if (lightSample.solidAnglePdf <= 0)
        return 0;

    float3 L = normalize(lightSample.position - surface.worldPos);

    // Ignore light samples that are below the geometric surface (but above the normal mapped surface)
    if (dot(L, surface.worldNormal) <= 0) //todo change with geoNormal
        return 0;

    float3 V = surface.viewDir;
    
    float diffuse = Lambert(surface.worldNormal, L);
    float3 F;
    float3 specular = SpecularBRDF(surface.worldNormal, V, L, surface.brdfData.Specular, surface.brdfData.Roughness, F);
    float3 reflectedRadiance = lightSample.radiance * (diffuse * surface.brdfData.Diffuse + specular);
    return reflectedRadiance / lightSample.solidAnglePdf;
}

// Compute the target PDF (p-hat) for the given light sample relative to a surface
float ReSTIR_GetLightSampleTargetPdfForSurface(LightSample lightSample, Surface surface)
{
    return CalculateLuminance(ReSTIR_ShadeSurfaceWithLightSample(lightSample, surface));
}


LightSample ReSTIR_SampleLight(LightInfo lightInfo, Surface surface, float2 uv)
{
    LightSample lightSample = CalculateLightSample(lightInfo, uv, surface.worldPos); 
    return lightSample;
}