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

//SSCS
/*
static const uint SSCS_MAX_STEPS = 16;
float SSCS(Light light, float3 viewPosition)
{
	float3 rayPosition = viewPosition;
	float2 rayUV = 0.0f;

	float4 rayProjected = mul(float4(rayPosition, 1.0f), FrameCB.projection);
	rayProjected.xy /= rayProjected.w;
	rayUV = rayProjected.xy * float2(0.5f, -0.5f) + 0.5f;

	float depth = depthTx.Sample(PointClampSampler, rayUV);
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
			depth = depthTx.Sample(PointClampSampler, rayUV);
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