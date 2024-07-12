#ifndef _LIGHTING_
#define _LIGHTING_

#include "CommonResources.hlsli"
#include "Common.hlsli"
#include "Constants.hlsli"
#include "BRDF.hlsli"
#include "DDGI/DDGICommon.hlsli"

#define DIRECTIONAL_LIGHT 0
#define POINT_LIGHT 1
#define SPOT_LIGHT 2

struct Light
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


///Shadows

float CalcShadowFactor_PCF3x3(SamplerComparisonState shadowSampler,
	Texture2D<float> shadowMap, float3 uvd, int shadowMapSize)
{
	if (uvd.z > 1.0f) return 1.0;

	float depth = uvd.z;
	const float dx = 1.0f / shadowMapSize;
	float2 offsets[9] =
	{
		float2(-dx, -dx),  float2(0.0f, -dx),  float2(dx, -dx),
		float2(-dx, 0.0f), float2(0.0f, 0.0f), float2(dx, 0.0f),
		float2(-dx, +dx),  float2(0.0f, +dx),  float2(dx, +dx)
	};

	float percentLit = 0.0f;
	[unroll(9)] 
	for (int i = 0; i < 9; ++i)
	{
		percentLit += shadowMap.SampleCmpLevelZero(shadowSampler,
			uvd.xy + offsets[i], depth);
	}
    percentLit /= 9.0f;
    return percentLit;
}

float GetShadowMapFactorWS(Light light, float3 worldPosition)
{
	StructuredBuffer<float4x4> lightViewProjections = ResourceDescriptorHeap[FrameCB.lightsMatricesIdx];
	bool castsShadows = light.shadowTextureIndex >= 0;
	float shadowFactor = 1.0f;
	if (castsShadows)
	{
		switch (light.type)
		{
		case DIRECTIONAL_LIGHT:
		{
			if (light.useCascades)
			{
				float4 viewPosition = mul(float4(worldPosition, 1.0f), FrameCB.view);
				float viewDepth = viewPosition.z;
				for (uint i = 0; i < 4; ++i)
				{
					float4x4 lightViewProjection = lightViewProjections[light.shadowMatrixIndex + i];
					float4 shadowMapPosition = mul(float4(worldPosition, 1.0f), lightViewProjection);
					float3 UVD = shadowMapPosition.xyz / shadowMapPosition.w;
					UVD.xy = 0.5 * UVD.xy + 0.5;
					UVD.y = 1.0 - UVD.y;

					if (viewDepth < FrameCB.cascadeSplits[i])
					{
						Texture2D<float> shadowMap = ResourceDescriptorHeap[NonUniformResourceIndex(light.shadowTextureIndex + i)];
						shadowFactor = CalcShadowFactor_PCF3x3(ShadowWrapSampler, shadowMap, UVD, 2048);
						break;
					}
				}
			}
			else
			{
				float4x4 lightViewProjection = lightViewProjections[light.shadowMatrixIndex];
				float4 shadowMapPosition = mul(float4(worldPosition, 1.0f), lightViewProjection);
				float3 UVD = shadowMapPosition.xyz / shadowMapPosition.w;
				UVD.xy = 0.5 * UVD.xy + 0.5;
				UVD.y = 1.0 - UVD.y;
				Texture2D<float> shadowMap = ResourceDescriptorHeap[NonUniformResourceIndex(light.shadowTextureIndex)];
				shadowFactor = CalcShadowFactor_PCF3x3(ShadowWrapSampler, shadowMap, UVD, 1024);
			}
		}
		break;
		case POINT_LIGHT:
		{
			float3 lightToPixelWS = worldPosition - light.position.xyz;
			uint cubeFaceIndex = GetCubeFaceIndex(lightToPixelWS);
			float4x4 lightViewProjection = lightViewProjections[light.shadowMatrixIndex + cubeFaceIndex];
			float4 shadowMapPosition = mul(float4(worldPosition, 1.0f), lightViewProjection);
			float3 UVD = shadowMapPosition.xyz / shadowMapPosition.w;
			UVD.xy = 0.5 * UVD.xy + 0.5;
			UVD.y = 1.0 - UVD.y;
			Texture2D<float> shadowMap = ResourceDescriptorHeap[NonUniformResourceIndex(light.shadowTextureIndex + cubeFaceIndex)];
			shadowFactor = CalcShadowFactor_PCF3x3(ShadowWrapSampler, shadowMap, UVD, 512);
		}
		break;
		case SPOT_LIGHT:
		{
			float4x4 lightViewProjection = lightViewProjections[light.shadowMatrixIndex];
			float4 shadowMapPosition = mul(float4(worldPosition, 1.0f), lightViewProjection);
			float3 UVD = shadowMapPosition.xyz / shadowMapPosition.w;
			UVD.xy = 0.5 * UVD.xy + 0.5;
			UVD.y = 1.0 - UVD.y;
			Texture2D<float> shadowMap = ResourceDescriptorHeap[NonUniformResourceIndex(light.shadowTextureIndex)];
			shadowFactor = CalcShadowFactor_PCF3x3(ShadowWrapSampler, shadowMap, UVD, 1024);
		}
		break;
		}
	}
	return shadowFactor;
}

float GetShadowMapFactor(Light light, float3 viewPosition)
{
	StructuredBuffer<float4x4> lightViewProjections = ResourceDescriptorHeap[FrameCB.lightsMatricesIdx];
	bool castsShadows = light.shadowTextureIndex >= 0;
	float shadowFactor = 1.0f;
	if (castsShadows)
	{
		switch (light.type)
		{
		case DIRECTIONAL_LIGHT:
		{
			if (light.useCascades)
			{
				float viewDepth = viewPosition.z;
				for (uint i = 0; i < 4; ++i)
				{
					float4 worldPosition = mul(float4(viewPosition, 1.0f), FrameCB.inverseView);
					worldPosition /= worldPosition.w;
					float4x4 lightViewProjection = lightViewProjections[light.shadowMatrixIndex + i];
					float4 shadowMapPosition = mul(worldPosition, lightViewProjection);
					float3 UVD = shadowMapPosition.xyz / shadowMapPosition.w;
					UVD.xy = 0.5 * UVD.xy + 0.5;
					UVD.y = 1.0 - UVD.y;

					if (viewDepth < FrameCB.cascadeSplits[i])
					{
						Texture2D<float> shadowMap = ResourceDescriptorHeap[NonUniformResourceIndex(light.shadowTextureIndex + i)];
						shadowFactor = CalcShadowFactor_PCF3x3(ShadowWrapSampler, shadowMap, UVD, 2048);
						break;
					}
				}
			}
			else
			{
				float4 worldPosition = mul(float4(viewPosition, 1.0f), FrameCB.inverseView);
				worldPosition /= worldPosition.w;
				float4x4 lightViewProjection = lightViewProjections[light.shadowMatrixIndex];
				float4 shadowMapPosition = mul(worldPosition, lightViewProjection);
				float3 UVD = shadowMapPosition.xyz / shadowMapPosition.w;
				UVD.xy = 0.5 * UVD.xy + 0.5;
				UVD.y = 1.0 - UVD.y;
				Texture2D<float> shadowMap = ResourceDescriptorHeap[NonUniformResourceIndex(light.shadowTextureIndex)];
				shadowFactor = CalcShadowFactor_PCF3x3(ShadowWrapSampler, shadowMap, UVD, 1024);
			}
		}
		break;
		case POINT_LIGHT:
		{
			float3 lightToPixelWS = mul(float4(viewPosition - light.position.xyz, 0.0f), FrameCB.inverseView).xyz;
			uint cubeFaceIndex = GetCubeFaceIndex(lightToPixelWS);
			float4 worldPosition = mul(float4(viewPosition, 1.0f), FrameCB.inverseView);
			worldPosition /= worldPosition.w;
			float4x4 lightViewProjection = lightViewProjections[light.shadowMatrixIndex + cubeFaceIndex];
			float4 shadowMapPosition = mul(worldPosition, lightViewProjection);
			float3 UVD = shadowMapPosition.xyz / shadowMapPosition.w;
			UVD.xy = 0.5 * UVD.xy + 0.5;
			UVD.y = 1.0 - UVD.y;
			Texture2D<float> shadowMap = ResourceDescriptorHeap[NonUniformResourceIndex(light.shadowTextureIndex + cubeFaceIndex)];
			shadowFactor = CalcShadowFactor_PCF3x3(ShadowWrapSampler, shadowMap, UVD, 512);
		}
		break;
		case SPOT_LIGHT:
		{
			float4 worldPosition = mul(float4(viewPosition, 1.0f), FrameCB.inverseView);
			worldPosition /= worldPosition.w;
			float4x4 lightViewProjection = lightViewProjections[light.shadowMatrixIndex];
			float4 shadowMapPosition = mul(worldPosition, lightViewProjection);
			float3 UVD = shadowMapPosition.xyz / shadowMapPosition.w;
			UVD.xy = 0.5 * UVD.xy + 0.5;
			UVD.y = 1.0 - UVD.y;
			Texture2D<float> shadowMap = ResourceDescriptorHeap[NonUniformResourceIndex(light.shadowTextureIndex)];
			shadowFactor = CalcShadowFactor_PCF3x3(ShadowWrapSampler, shadowMap, UVD, 1024);
		}
		break;
		}
	}
	return shadowFactor;
}

float GetRayTracedShadowsFactor(Light light, float2 uv)
{
	bool rayTracedShadows = light.shadowMaskIndex >= 0;
	if(rayTracedShadows)
    {
        Texture2D<float> rayTracedMaskTexture = ResourceDescriptorHeap[NonUniformResourceIndex(light.shadowMaskIndex)];
        float maskValue = rayTracedMaskTexture.SampleLevel(LinearWrapSampler, uv, 0).r;
        return maskValue;
    }
	return 1.0f;
}


///Lighting

struct LightingResult
{
	float3 Diffuse;
	float3 Specular;

	LightingResult operator+(LightingResult res2) 
	{
		LightingResult res = (LightingResult)0;
		res.Diffuse  = Diffuse + res2.Diffuse;
		res.Specular = Specular + res2.Specular;
		return res;
	}
};


float DoAttenuation(float distance, float range)
{
	float att = saturate(1.0f - (distance * distance / (range * range)));
	return att * att;
}

float GetLightAttenuation(Light light, float3 P, out float3 L)
{
	L = normalize(light.position.xyz - P);
	float attenuation = 1.0f;
	if(light.type == POINT_LIGHT)
	{
		float distance = length(light.position.xyz - P);
		attenuation = DoAttenuation(distance, light.range);
	}
	else if(light.type == SPOT_LIGHT)
	{
		float distance = length(light.position.xyz - P);
		attenuation = DoAttenuation(distance, light.range);

		float3 normalizedLightDir = normalize(light.direction.xyz);
		float cosAng = dot(-normalizedLightDir, L);
		float conAtt = saturate((cosAng - light.outerCosine) / (light.innerCosine - light.outerCosine));
		conAtt *= conAtt;

		attenuation *= conAtt;
	}
	return attenuation;
}


LightingResult DefaultLitBxDF(float3 diffuseColor, float3 specularColor, float specularRoughness, float3 N, float3 V, float3 L, float attenuation)
{
	LightingResult lighting = (LightingResult)0;
	if(attenuation <= 0.0f) return lighting;

	float NdotL = saturate(dot(N, L));
	if(NdotL == 0.0f) return lighting;

	float3 H = normalize(V + L);
	float a = specularRoughness * specularRoughness;
	float a2 = clamp(a * a, 0.0001f, 1.0f);

	float D = D_GGX(N, H, a);
	float Vis = V_SmithJointApprox(N, V, L, a);
	float3 F = F_Schlick(V, H, specularColor);

	lighting.Specular = (attenuation * NdotL) * (D * Vis) * F;
	lighting.Diffuse = (attenuation * NdotL) * DiffuseBRDF(diffuseColor);

	return lighting;
}

LightingResult DoLightNoShadows(Light light, float3 P, float3 N, float3 V, float3 albedo, float metallic, float roughness)
{
	LightingResult result = (LightingResult)0;
	
	float3 L;
	float attenuation = GetLightAttenuation(light, P, L);
	if(attenuation <= 0.0f) return result;

	BrdfData brdfData = GetBrdfData(albedo, metallic, roughness);
	result = DefaultLitBxDF(brdfData.Diffuse, brdfData.Specular, brdfData.Roughness, N, V, L, attenuation);

	result.Diffuse  *= light.color.rgb;
	result.Specular *= light.color.rgb;
	return result;
}

LightingResult DoLight(Light light, BrdfData brdfData, float3 P, float3 N, float3 V, float2 uv)
{
	LightingResult result = (LightingResult)0;
	
	float3 L;
	float attenuation = GetLightAttenuation(light, P, L);
	if(attenuation <= 0.0f) return result;

	attenuation *= GetShadowMapFactor(light, P);
	attenuation *= GetRayTracedShadowsFactor(light, uv);
	if(attenuation <= 0.0f) return result;

	result = DefaultLitBxDF(brdfData.Diffuse, brdfData.Specular, brdfData.Roughness, N, V, L, attenuation);

	result.Diffuse  *= light.color.rgb;
	result.Specular *= light.color.rgb;
	return result;
}

float3 GetIndirectLighting(float3 viewPosition, float3 viewNormal, float3 diffuseColor, float ambientOcclusion)
{
	float3 indirectLighting = 0.0f;
	int ddgiVolumesIdx = FrameCB.ddgiVolumesIdx;
	if (ddgiVolumesIdx >= 0)
	{
		StructuredBuffer<DDGIVolume> ddgiVolumes = ResourceDescriptorHeap[ddgiVolumesIdx];
		DDGIVolume ddgiVolume = ddgiVolumes[0];
		
		float3 worldNormal = normalize(mul(viewNormal, (float3x3) FrameCB.inverseView));
		float4 worldPosition = mul(float4(viewPosition, 1.0f), FrameCB.inverseView);
		worldPosition /= worldPosition.w;
		float3 Wo = normalize(FrameCB.cameraPosition.xyz - worldPosition.xyz);
		indirectLighting = DiffuseBRDF(diffuseColor) * ambientOcclusion * SampleDDGIIrradiance(ddgiVolume, worldPosition.xyz, worldNormal, -Wo);
	}
	else
	{
		indirectLighting = 0.1f * FrameCB.ambientColor.rgb * diffuseColor * ambientOcclusion;
	}
	return indirectLighting;
}

#endif


//SSCS
/*
static const uint SSCS_MAX_STEPS = 16;
float ScreenSpaceShadows(Light light, float3 viewPosition)
{
	float3 rayPosition = viewPosition;
	float2 rayUV = 0.0f;

	float4 rayProjected = mul(float4(rayPosition, 1.0f), FrameCB.projection);
	rayProjected.xy /= rayProjected.w;
	rayUV = rayProjected.xy * float2(0.5f, -0.5f) + 0.5f;

	float depth = depthTexture.Sample(PointClampSampler, rayUV);
	float linearDepth = LinearizeDepth(depth);

	const float SSCS_STEP_LENGTH = light.sscsMaxRayDistance / (float)SSCS_MAX_STEPS;
	if (linearDepth > light.sscsMaxDepthDistance) return 1.0f;

	float3 rayDirection = normalize(-light.direction.xyz);
	float3 rayStep = rayDirection * SSCS_STEP_LENGTH;

	float occlusion = 0.0f;
	[unroll(SSCS_MAX_STEPS)]
	for (uint i = 0; i < SSCS_MAX_STEPS; i++)
	{
		// Step the ray
		rayPosition += rayStep;

		rayProjected = mul(float4(rayPosition, 1.0), FrameCB.projection);
		rayProjected.xy /= rayProjected.w;
		rayUV = rayProjected.xy * float2(0.5f, -0.5f) + 0.5f;

		[branch]
		if (IsSaturated(rayUV))
		{
			depth = depthTexture.Sample(PointClampSampler, rayUV);
			linearDepth = LinearizeDepth(depth);
			float depthDelta = rayProjected.z - linearDepth;

			if (depthDelta > 0 && (depthDelta < light.sscsThickness))
			{
				occlusion = 1.0f;
				float2 fade = max(12 * abs(rayUV - 0.5) - 5, 0);
				occlusion *= saturate(1 - dot(fade, fade));
				break;
			}
		}
	}
	return 1.0f - occlusion;
}
*/