#ifndef _LIGHTING_
#define _LIGHTING_

#include "CommonResources.hlsli"
#include "Common.hlsli"
#include "Constants.hlsli"
#include "BRDF.hlsli"
#include "Packing.hlsli"
#include "Scene.hlsli"
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


///Lighting

float GetShadowMapFactor(Light light, float3 viewPosition);
float GetRayTracedShadowsFactor(Light light, float2 uv);

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

float GetAttenuation(Light light, float3 P, float2 uv, out float3 L)
{
	float attenuation = GetLightAttenuation(light, P, L);
	if(attenuation <= 0.0f) return 0.0f;
	attenuation *= GetShadowMapFactor(light, P);
	attenuation *= GetRayTracedShadowsFactor(light, uv);
	return attenuation;
}

float3 DoLightNoShadows_Default(Light light, float3 P, float3 N, float3 V, float3 albedo, float metallic, float roughness)
{
	float3 L;
	float attenuation = GetLightAttenuation(light, P, L);
	if(attenuation <= 0.0f) return 0.0f;

	float NdotL = saturate(dot(N, L));
	if(NdotL == 0.0f) return 0.0f;

	BrdfData brdfData = GetBrdfData(albedo, metallic, roughness);
    float3 brdf = DefaultBRDF(L, V, N, brdfData.Diffuse, brdfData.Specular, brdfData.Roughness);
	return brdf * NdotL * light.color.rgb * attenuation;
}

float3 DoLight_Sheen(Light light, BrdfData brdfData, float3 P, float3 N, float3 V, float2 uv, float4 sheenData)
{
	float3 L;
	float attenuation = GetAttenuation(light, P, uv, L);
	if(attenuation <= 0.0f) return 0.0f;

	float NdotL = saturate(dot(N, L));
	if(NdotL == 0.0f) return 0.0f;

	float3 sheenColor = sheenData.xyz;
    float  sheenRoughness = sheenData.w;
    float3 sheenBRDF = SheenBRDF(L, V, N, sheenColor, sheenRoughness);

	Texture2D sheenETexture = ResourceDescriptorHeap[FrameCB.sheenEIdx];
    float  sheenScaling = SheenScale(V, N, sheenColor, sheenRoughness, sheenETexture);

    float3 brdf = sheenBRDF + sheenScaling * DefaultBRDF(L, V, N, brdfData.Diffuse, brdfData.Specular, brdfData.Roughness);
    return brdf * light.color.rgb * NdotL * attenuation;
}

float3 DoLight_ClearCoat(Light light, BrdfData brdfData, float3 P, float3 N, float3 V, float2 uv, float clearCoat, float clearCoatRoughness, float3 clearCoatNormal)
{
	float3 L;
	float attenuation = GetAttenuation(light, P, uv, L);
	if(attenuation <= 0.0f) return  0.0f;

	float clearCoatNdotL = saturate(dot(clearCoatNormal, L));
	float3 clearCoatSpecular = 0.04f;

	float3 clearCoatF;
    float3 clearCoatBRDF = SpecularBRDF(clearCoatNormal, V, L, clearCoatSpecular, clearCoatRoughness, clearCoatF);
    float3 baseBRDF = DefaultBRDF(L, V, N, brdfData.Diffuse, brdfData.Specular, brdfData.Roughness);
     
    float NdotL = saturate(dot(N, L));
    float3 brdf = baseBRDF * (1.0 - clearCoat * clearCoatF) * NdotL + clearCoatBRDF * clearCoat * clearCoatNdotL;
    return brdf * light.color.rgb * attenuation;
}

float3 DoLight_Anisotropy(Light light, BrdfData brdfData, float3 P, float3 N, float3 V, float2 uv, float4 customData)
{
	float3 L;
    float attenuation = GetAttenuation(light, P, uv, L);
    if (attenuation <= 0.0f) return 0.0f;

    float NdotL = saturate(dot(N, L));
    if (NdotL == 0.0f) return 0.0f;

	float3  anisotropicT = customData.xyz;
	float   anisotropyStrength = customData.w;

	float3 anisotropicB = normalize(cross(N, anisotropicT));
    float TdotL = dot(anisotropicT, L);
    float BdotL = dot(anisotropicB, L);
	float3 H = normalize(V + L);
    float TdotH = dot(anisotropicT, H);
    float BdotH = dot(anisotropicB, H);

    float ab = brdfData.Roughness * brdfData.Roughness;
    float at = lerp(ab, 1.0f, anisotropyStrength * anisotropyStrength);

	float3 D = D_GGX_Aniso(at, ab, anisotropicT, anisotropicB, N, H);
    float3 Vis = V_SmithGGX_Aniso(at, ab, anisotropicT, anisotropicB, N, V, L);
    float3 F = F_Schlick(V, H, brdfData.Specular);

    float3 brdf = DiffuseBRDF(brdfData.Diffuse) * (1.0 - F) + D * Vis * F;
    return brdf * light.color.rgb * NdotL * attenuation;
}

float3 DoLight_Default(Light light, BrdfData brdfData, float3 P, float3 N, float3 V, float2 uv)
{
	float3 L;
	float attenuation = GetAttenuation(light, P, uv, L);
	if(attenuation <= 0.0f) return  0.0f;

	float NdotL = saturate(dot(N, L));
	if(NdotL == 0.0f) return 0.0f;

    float3 brdf = DefaultBRDF(L, V, N, brdfData.Diffuse, brdfData.Specular, brdfData.Roughness);
	return brdf * NdotL * attenuation * light.color.rgb;
}

float3 DoLight(uint extension, Light light, BrdfData brdfData, float3 P, float3 N, float3 V, float2 uv, float4 customData)
{
	float3 result = 0.0f;
	switch(extension)
	{
	case ShadingExtension_ClearCoat:
	{
		float clearCoat, clearCoatRoughness;
		float3 clearCoatNormal;
		DecodeClearCoat(customData, clearCoat, clearCoatRoughness, clearCoatNormal);
		result = DoLight_ClearCoat(light, brdfData, P, N, V, uv, clearCoat, clearCoatRoughness, clearCoatNormal);
	}
	break;
	case ShadingExtension_Sheen: 
	{
		result = DoLight_Sheen(light, brdfData, P, N, V, uv, customData); 
	}
	break;
	case ShadingExtension_Anisotropy:
	{
		DoLight_Anisotropy(light, brdfData, P, N, V, uv, customData);
	}
	break;
	case ShadingExtension_Default: 
	case ShadingExtension_Max:
	default:
		result = DoLight_Default(light, brdfData, P, N, V, uv); break;
	}
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

float3 GetIndirectLightingWS(float3 worldPosition, float3 worldNormal, float3 diffuseColor, float ambientOcclusion)
{
	float3 indirectLighting = 0.0f;
	int ddgiVolumesIdx = FrameCB.ddgiVolumesIdx;
	if (ddgiVolumesIdx >= 0)
	{
		StructuredBuffer<DDGIVolume> ddgiVolumes = ResourceDescriptorHeap[ddgiVolumesIdx];
		DDGIVolume ddgiVolume = ddgiVolumes[0];
		float3 Wo = normalize(FrameCB.cameraPosition.xyz - worldPosition.xyz);
		indirectLighting = DiffuseBRDF(diffuseColor) * ambientOcclusion * SampleDDGIIrradiance(ddgiVolume, worldPosition.xyz, worldNormal, -Wo);
	}
	else
	{
		indirectLighting = 0.1f * FrameCB.ambientColor.rgb * diffuseColor * ambientOcclusion;
	}
	return indirectLighting;
}




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


#endif
