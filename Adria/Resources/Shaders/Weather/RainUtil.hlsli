#ifndef  _RAIN_UTIL_
#define  _RAIN_UTIL_
#include "CommonResources.hlsli"

bool IsBlockedFromRain(float3 worldPosition)
{
	matrix rainViewProjection = FrameCB.rainViewProjection;
	float4 rainMapPosition = mul(float4(worldPosition, 1.0f), rainViewProjection);
	float3 UVD = rainMapPosition.xyz / rainMapPosition.w;
	UVD.xy = 0.5 * UVD.xy + 0.5;
	UVD.y = 1.0 - UVD.y;
	Texture2D<float> blockerMap = ResourceDescriptorHeap[FrameCB.rainBlockerMapIdx];
	if (UVD.z > 1.0f) return true;
	float depth = UVD.z;
	return !blockerMap.SampleCmpLevelZero(ShadowWrapSampler, UVD.xy, depth);
}


void ApplyRain(in float3 worldPosition, inout float3 albedoColor, inout float roughness, inout float3 normal, in float3 tangent, in float3 bitangent)
{
	const float wetFactor = saturate(5.0f * saturate(normal.y));
	if(!IsBlockedFromRain(worldPosition))
	{
		Texture3D rainSplashDiffuseTx = ResourceDescriptorHeap[FrameCB.rainSplashDiffuseIdx];
		Texture3D rainSplashBumpTx = ResourceDescriptorHeap[FrameCB.rainSplashBumpIdx];
		float3 rainSplashDiffuse = rainSplashDiffuseTx.SampleLevel(LinearMirrorSampler, float3(worldPosition.xz / 5.0f, FrameCB.rainTotalTime), 0).rgb;
		float3 rainSplashBump = rainSplashBumpTx.SampleLevel(LinearMirrorSampler, float3(worldPosition.xz  / 10.0f, FrameCB.rainTotalTime), 0).rgb - 0.5f;
		normal += wetFactor * 2 * (rainSplashBump.x * tangent + rainSplashBump.y * bitangent);
		albedoColor += wetFactor * rainSplashDiffuse;
	}
	albedoColor *= lerp(1.0f, 0.3f, wetFactor);
	roughness = saturate(lerp(roughness, roughness * 2.5f, wetFactor));
}

#endif