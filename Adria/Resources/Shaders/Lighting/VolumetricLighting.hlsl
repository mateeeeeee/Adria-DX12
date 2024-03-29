#include "CommonResources.hlsli"
#include "Common.hlsli"
#include "Lighting.hlsli"
#include "DitherUtil.hlsli"

#define BLOCK_SIZE 16

struct VolumetricLightingConstants
{
	uint depthIdx;
	uint outputIdx;
	uint resolutionFactor;
};
ConstantBuffer<VolumetricLightingConstants> PassCB : register(b1);

float GetAttenuation(Light light, float3 P);

struct CSInput
{
	uint3 GroupId : SV_GroupID;
	uint3 GroupThreadId : SV_GroupThreadID;
	uint3 DispatchThreadId : SV_DispatchThreadID;
	uint  GroupIndex : SV_GroupIndex;
};

[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void VolumetricLightingCS(CSInput input)
{
	Texture2D<float>        depthTx = ResourceDescriptorHeap[PassCB.depthIdx];
	StructuredBuffer<Light> lights	= ResourceDescriptorHeap[FrameCB.lightsIdx];
	RWTexture2D<float4> outputTx = ResourceDescriptorHeap[PassCB.outputIdx];

	uint lightCount, unused;
	lights.GetDimensions(lightCount, unused);
	uint2 resolution = uint2(FrameCB.renderResolution) >> PassCB.resolutionFactor;

	float2 uv = ((float2) input.DispatchThreadId.xy + 0.5f) * 1.0f / resolution;
	float depth = depthTx.SampleLevel(LinearClampSampler, uv, 2);

	float3 viewPosition = GetViewPosition(uv, depth);
	float3 cameraPosition = float3(0.0f, 0.0f, 0.0f);
	float3 V = cameraPosition - viewPosition;
	float cameraDistance = length(V);
	V /= cameraDistance;

	const uint sampleCount = 16;
	const float stepSize = cameraDistance / sampleCount;
    viewPosition = viewPosition + V * stepSize * Dither(((float2)input.DispatchThreadId.xy + 0.5f));

	float3 totalAccumulation = 0.0f;
	for (uint i = 0; i < lightCount; ++i)
	{
		Light light = lights[i];
		if (!light.active || !light.volumetric || light.shadowTextureIndex < 0) continue;

		float3 P = viewPosition;
		float3 lightAccumulation = 0.0f;
		float marchedDistance = 0.0f;

		for (uint j = 0; j < sampleCount; ++j)
		{
			lightAccumulation += GetAttenuation(light, P);
			marchedDistance += stepSize;
			P = P + V * stepSize;
        }

		lightAccumulation /= sampleCount;
		totalAccumulation += lightAccumulation * light.color.rgb * light.volumetricStrength;
	}

	outputTx[input.DispatchThreadId.xy] = float4(totalAccumulation, 1.0f);
}


float GetAttenuation(Light light, float3 P)
{
	StructuredBuffer<float4x4> lightViewProjections = ResourceDescriptorHeap[FrameCB.lightsMatricesIdx];
	float attenuation = 0.0f;
	if (light.type == DIRECTIONAL_LIGHT)
	{
		if (light.useCascades)
		{
			for (uint i = 0; i < 4; ++i)
			{
				float4 worldPosition = mul(float4(P, 1.0f), FrameCB.inverseView);
				worldPosition /= worldPosition.w;
				float4x4 lightViewProjection = lightViewProjections[light.shadowMatrixIndex + i];
				float4 shadowMapPosition = mul(worldPosition, lightViewProjection);
				float3 UVD = shadowMapPosition.xyz / shadowMapPosition.w;
				UVD.xy = 0.5 * UVD.xy + 0.5;
				UVD.y = 1.0 - UVD.y;

                if (P.z < FrameCB.cascadeSplits[i])
				{
					Texture2D<float> shadowMap = ResourceDescriptorHeap[NonUniformResourceIndex(light.shadowTextureIndex + i)];
					attenuation = CalcShadowFactor_PCF3x3(ShadowWrapSampler, shadowMap, UVD, 2048);
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
			[branch]
			if (IsSaturated(UVD.xy))
			{
				Texture2D<float> shadowMap = ResourceDescriptorHeap[NonUniformResourceIndex(light.shadowTextureIndex)];
				attenuation = CalcShadowFactor_PCF3x3(ShadowWrapSampler, shadowMap, UVD, 1024);
			}
		}
	}
	else if (light.type == POINT_LIGHT)
	{
        float distanceToLight = distance(light.position.xyz, P);
        attenuation = DoAttenuation(distanceToLight, light.range);
		
		float3 lightToPixelWS = mul(float4(P - light.position.xyz, 0.0f), FrameCB.inverseView).xyz;
		uint cubeFaceIndex = GetCubeFaceIndex(lightToPixelWS);
		float4 worldPosition = mul(float4(P, 1.0f), FrameCB.inverseView);
		worldPosition /= worldPosition.w;
		float4x4 lightViewProjection = lightViewProjections[light.shadowMatrixIndex + cubeFaceIndex];
		float4 shadowMapPosition = mul(worldPosition, lightViewProjection);
		float3 UVD = shadowMapPosition.xyz / shadowMapPosition.w;
		UVD.xy = 0.5 * UVD.xy + 0.5;
		UVD.y = 1.0 - UVD.y;
		[branch]
		if (IsSaturated(UVD.xy))
		{
			Texture2D<float> shadowMap = ResourceDescriptorHeap[NonUniformResourceIndex(light.shadowTextureIndex + cubeFaceIndex)];
			attenuation *= CalcShadowFactor_PCF3x3(ShadowWrapSampler, shadowMap, UVD, 512);
		}
	}
	else if (light.type == SPOT_LIGHT)
	{
        float3 L = light.position.xyz - P;
        float distanceToLight = length(L);
        L /= distanceToLight;
		
        float spotFactor = dot(L, normalize(-light.direction.xyz));
        float spotCutOff = light.outerCosine;

		[branch]
        if (spotFactor > spotCutOff)
        {
            attenuation = DoAttenuation(distanceToLight, light.range);
			
            float conAttenuation = saturate((spotFactor - light.outerCosine) / (light.innerCosine - light.outerCosine));
            conAttenuation *= conAttenuation;
            attenuation *= conAttenuation;
			
            float4 worldPosition = mul(float4(P, 1.0f), FrameCB.inverseView);
            worldPosition /= worldPosition.w;
            float4x4 lightViewProjection = lightViewProjections[light.shadowMatrixIndex];
            float4 shadowMapPosition = mul(worldPosition, lightViewProjection);
            float3 UVD = shadowMapPosition.xyz / shadowMapPosition.w;
            UVD.xy = 0.5 * UVD.xy + 0.5;
            UVD.y = 1.0 - UVD.y;
			[branch]
            if (IsSaturated(UVD.xy))
            {
                Texture2D<float> shadowMap = ResourceDescriptorHeap[NonUniformResourceIndex(light.shadowTextureIndex)];
                attenuation = CalcShadowFactor_PCF3x3(ShadowWrapSampler, shadowMap, UVD, 1024);
            }
        }
	}
	return attenuation;
}