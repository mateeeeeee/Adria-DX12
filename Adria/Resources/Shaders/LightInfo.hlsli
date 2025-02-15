#pragma once
#ifndef _LIGHTINFO_
#define _LIGHTINFO_
#include "CommonResources.hlsli"

#define DIRECTIONAL_LIGHT 0
#define POINT_LIGHT 1
#define SPOT_LIGHT 2

struct LightInfo
{
	float4	position;
	float4	direction;
	float4	color;
	
	int		active;
	float	range;
	int		type;
	float	outerCosine;
	
	float	innerCosine;
	int     volumetric;
	float   volumetricStrength;
	int     useCascades;
	
	int     shadowTextureIndex;
	int     shadowMatrixIndex;
    int     shadowMaskIndex;
    int     padd;
};

static StructuredBuffer<LightInfo> lightBuffer = ResourceDescriptorHeap[FrameCB.lightsIdx];
LightInfo LoadLightInfo(uint lightIndex)
{
	return lightBuffer[lightIndex];
}

struct LightSample
{
    float3 position;
    float3 normal;
    float3 radiance;
    float  solidAnglePdf;
};
LightSample EmptyLightSample()
{
    return (LightSample)0;
}

struct LightShaping
{
    float cosConeAngle;
    float3 primaryAxis;
    float cosConeSoftness;
    uint isSpot;
    int iesProfileIndex;
};

struct PointLight
{
    float3 position;
    float3 flux;
    //LightShaping shaping;

    // Interface methods
    LightSample CalculateSample(in const float3 viewerPosition)
    {
        const float3 lightVector = position - viewerPosition;
        
        LightSample lightSample;
        // We cannot compute finite values for radiance and solidAnglePdf for a point light,
        // so return the limit of (radiance / solidAnglePdf) with radius --> 0 as radiance.
        lightSample.position = position;
        lightSample.normal = normalize(-lightVector);
        lightSample.radiance = flux / dot(lightVector, lightVector);
        lightSample.solidAnglePdf = 1.0;

        return lightSample;
    }

    static PointLight Create(in const LightInfo lightInfo)
    {
        PointLight pointLight;
        pointLight.position = lightInfo.position.xyz;
        pointLight.flux = lightInfo.color.rgb;
        return pointLight;
    }
};


LightSample CalculateLightSample(in const LightInfo lightInfo, in float2 randomUv, in const float3 viewerPosition)
{
    LightSample lightSample = EmptyLightSample();
    switch (lightInfo.type)
    {
    case DIRECTIONAL_LIGHT:      break;
    case POINT_LIGHT:            
    case SPOT_LIGHT:             lightSample = PointLight::Create(lightInfo).CalculateSample(viewerPosition); break;
    }

    //if (lightSample.solidAnglePdf > 0)
    //{
    //    lightSample.radiance *= evaluateLightShaping(unpackLightShaping(lightInfo),
    //        viewerPosition, lightSample.position);
    //}

    return lightSample;
}

#endif