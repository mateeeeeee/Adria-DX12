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
ConstantBuffer<VolumetricLightingConstants> VolumetricLightingPassCB : register(b1);

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
	StructuredBuffer<Light> lightBuffer	= ResourceDescriptorHeap[FrameCB.lightsIdx];
	Texture2D<float>        depthTexture = ResourceDescriptorHeap[VolumetricLightingPassCB.depthIdx];
	RWTexture2D<float4> outputTexture = ResourceDescriptorHeap[VolumetricLightingPassCB.outputIdx];

	uint lightCount, unused;
	lightBuffer.GetDimensions(lightCount, unused);
	uint2 resolution = uint2(FrameCB.renderResolution) >> VolumetricLightingPassCB.resolutionFactor;

	float2 uv = ((float2) input.DispatchThreadId.xy + 0.5f) * 1.0f / resolution;
	float depth = depthTexture.SampleLevel(LinearClampSampler, uv, 2);

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
		Light light = lightBuffer[i];
		if (!light.active || !light.volumetric) continue;

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

	outputTexture[input.DispatchThreadId.xy] = float4(totalAccumulation, 1.0f);
}


float GetAttenuation(Light light, float3 P)
{
	float3 L;
	float attenuation = GetLightAttenuation(light, P, L);
	if(attenuation <= 0.0f) return 0.0f;

	float shadowFactor = GetShadowMapFactor(light, P);
	attenuation *= shadowFactor;
	return attenuation;
}