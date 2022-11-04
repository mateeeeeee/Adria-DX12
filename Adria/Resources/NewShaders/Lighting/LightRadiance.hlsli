#include "../Lighting.hlsli"
#include "../CommonResources.hlsli"
#include "../Common.hlsli"

static float3 LightRadiance(Light light, float3 P, float3 N, float3 V, float3 albedo, float metallic, float roughness)
{
	StructuredBuffer<float4x4> lightViewProjections = ResourceDescriptorHeap[FrameCB.lightsMatricesIdx];
	float3 lightRadiance = 0.0f;
	switch (light.type)
	{
	case DIRECTIONAL_LIGHT:
		lightRadiance = DirectionalLightPBR(light, P, N, V, albedo, metallic, roughness);
		break;
	case POINT_LIGHT:
		lightRadiance = PointLightPBR(light, P, N, V, albedo, metallic, roughness);
		break;
	case SPOT_LIGHT:
		lightRadiance = SpotLightPBR(light, P, N, V, albedo, metallic, roughness);
		break;
	}
	bool castsShadows = light.shadowTextureIndex >= 0;
	if (castsShadows)
	{
		float shadowFactor = 1.0f;
		switch (light.type)
		{
		case DIRECTIONAL_LIGHT:
		{
			if (light.useCascades)
			{
				float viewDepth = P.z;
				for (uint i = 0; i < 4; ++i)
				{
					float4 worldPosition = mul(float4(P, 1.0f), FrameCB.inverseView);
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
				float4 worldPosition = mul(float4(P, 1.0f), FrameCB.inverseView);
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
			float3 lightToPixelWS = mul(float4(P - light.position.xyz, 0.0f), FrameCB.inverseView).xyz;
			uint cubeFaceIndex = GetCubeFaceIndex(lightToPixelWS);
			float4 worldPosition = mul(float4(P, 1.0f), FrameCB.inverseView);
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
			float4 worldPosition = mul(float4(P, 1.0f), FrameCB.inverseView);
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
		lightRadiance *= shadowFactor;
	}

	return lightRadiance;
}