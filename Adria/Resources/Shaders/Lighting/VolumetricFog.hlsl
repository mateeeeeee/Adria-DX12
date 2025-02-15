#include "CommonResources.hlsli"
#include "Noise.hlsli"
#include "Lighting.hlsli"

#define BLUE_NOISE_TEXTURE_SIZE 128
#define VOXEL_GRID_SIZE_Z		128


struct CSInput
{
	uint3 GroupId : SV_GroupID;
	uint3 GroupThreadId : SV_GroupThreadID;
	uint3 DispatchThreadId : SV_DispatchThreadID;
	uint  GroupIndex : SV_GroupIndex;
};

struct FogVolume
{
	float3  center;
	float3  extents;
	float3  color;
	float   densityBase;
	float   densityChange;
};


struct LightInjectionConstants
{
	uint3  voxelGridDimensions;
	uint   fogVolumesCount;
	uint   fogVolumeBufferIdx;
	uint   lightInjectionTargetIdx;
	uint   lightInjectionTargetHistoryIdx;
    uint   blueNoiseIdx;
};
ConstantBuffer<LightInjectionConstants> LightInjectionPassCB : register(b1);

//already exists in VolumetricClouds.hlsl, move volumetric common stuff to Volumetrics.hlsli
float HenyeyGreensteinPhase(float LdotV, float G)
{
	float result = 1.0f - G * G;
	result /= (4.0f * 3.1415f * pow(1.0f + G * G - (2.0f * G) * LdotV, 1.5f));
	return result;
}

float3 GetUVWFromVoxelCoords(uint3 voxelCoords, float jitter, uint3 voxelGridDimensions)
{
	float2 texelUV = ((float2)voxelCoords.xy + 0.5f) / voxelGridDimensions.xy;
	float z = (float)(voxelCoords.z + 0.5f + jitter) / voxelGridDimensions.z;
	z = saturate(z);
	const float n = FrameCB.cameraNear;
	const float f = FrameCB.cameraFar; 
	float linearDepth = f + z * z * (n - f);
	return float3(texelUV, DelinearizeDepth(linearDepth));
}

float3 GetWorldPositionFromVoxelCoords(uint3 voxelCoords, float jitter, uint3 voxelGridDimensions)
{
	float3 uvw = GetUVWFromVoxelCoords(voxelCoords, jitter, voxelGridDimensions);
	return GetWorldPosition(uvw.xy, uvw.z);
}

float SampleBlueNoise(uint3 coords)
{
	int2 noiseCoords = (coords.xy + int2(0, 1) * coords.z * BLUE_NOISE_TEXTURE_SIZE) % BLUE_NOISE_TEXTURE_SIZE;
	Texture2D<float> blueNoiseTexture = ResourceDescriptorHeap[LightInjectionPassCB.blueNoiseIdx];
    return blueNoiseTexture.Load(int3(noiseCoords, 0));
}

[numthreads(8, 8, 8)]
void LightInjectionCS(CSInput input)
{
	uint3 voxelGridCoords = input.DispatchThreadId;
	if(any(voxelGridCoords >= LightInjectionPassCB.voxelGridDimensions)) return;

	const float jitter	 = (SampleBlueNoise(voxelGridCoords) - 0.5f);
    float3 worldPosition = GetWorldPositionFromVoxelCoords(voxelGridCoords, jitter, LightInjectionPassCB.voxelGridDimensions);

	float3 cellAbsorption = 0.0f;
	float  cellDensity	  = 0.0f;

	StructuredBuffer<FogVolume> fogVolumeBuffer = ResourceDescriptorHeap[LightInjectionPassCB.fogVolumeBufferIdx];
	for(uint i = 0; i < LightInjectionPassCB.fogVolumesCount; ++i)
	{
		FogVolume fogVolume = fogVolumeBuffer[i];

		float3 localPos = (worldPosition - fogVolume.center) / fogVolume.extents;
		float normalizedHeight = localPos.y * 0.5f + 0.5f;
		if(any(localPos < -1) || any(localPos > 1))
		{
			continue;
		}

		float density = min(1.0f, fogVolume.densityBase + (1.0f - normalizedHeight) * (1.0f - normalizedHeight) * fogVolume.densityChange);
		if(density < 0.0f)
		{
			continue;
		}

		cellAbsorption += density * fogVolume.color;
		cellDensity += density;
	}

	float3 inScattering = cellAbsorption;
	float3 totalLighting = 0.0f;
	float3 viewDirection = normalize(FrameCB.cameraPosition.xyz - worldPosition);

	if(any(inScattering > 0.0f))
	{
		for (int i = 0; i < FrameCB.lightCount; ++i)
		{
			LightInfo lightInfo = LoadLightInfo(i);
			if (!lightInfo.active || !lightInfo.volumetric) continue;

			float3 L;
			float attenuation = GetLightAttenuation(lightInfo, worldPosition, L);
			if(attenuation <= 0.0f) continue;

			float shadowFactor = GetShadowMapFactorWS(lightInfo, worldPosition);
			attenuation *= shadowFactor;
			if(attenuation <= 0.0f) continue;

			float VdotL = dot(viewDirection, L);
			totalLighting += attenuation * lightInfo.color.rgb * saturate(HenyeyGreensteinPhase(VdotL, 0.3f));
		}
	}

	float4 currentScattering = float4(inScattering * totalLighting, cellDensity);

	float3 worldPositionNoJitter = GetWorldPositionFromVoxelCoords(voxelGridCoords, 0.0f, LightInjectionPassCB.voxelGridDimensions);
	float4 prevNDC = mul(float4(worldPositionNoJitter, 1), FrameCB.prevViewProjection);
	prevNDC.xyz /= prevNDC.w;
	float3 prevUVW = float3(prevNDC.x * 0.5f + 0.5f, -prevNDC.y * 0.5f + 0.5f, prevNDC.z);
	prevUVW.z = LinearizeDepth(prevUVW.z);
	prevUVW.z = sqrt((prevUVW.z - FrameCB.cameraFar) / (FrameCB.cameraNear - FrameCB.cameraFar));

	Texture3D<float4> lightInjectionTargetHistory = ResourceDescriptorHeap[LightInjectionPassCB.lightInjectionTargetHistoryIdx];
	float4 prevScattering = lightInjectionTargetHistory.SampleLevel(LinearClampSampler, prevUVW, 0);
	
	float lerpFactor = 0.02f;
	if(any(prevUVW < 0.0f) || any(prevUVW > 1.0f)) lerpFactor = 1.0f;
	currentScattering = lerp(prevScattering, currentScattering, lerpFactor);
	
	RWTexture3D<float4> lightInjectionTarget = ResourceDescriptorHeap[LightInjectionPassCB.lightInjectionTargetIdx];
	lightInjectionTarget[voxelGridCoords] = currentScattering;
}

struct ScatteringIntegrationConstants
{
	uint3 voxelGridDimensions;
	uint  lightInjectionTargetIdx;
	uint  outputIdx;
};
ConstantBuffer<ScatteringIntegrationConstants> ScatteringIntegrationPassCB : register(b1);

[numthreads(8, 8, 8)]
void ScatteringIntegrationCS(CSInput input)
{
	Texture3D<float4> lightInjectionTarget = ResourceDescriptorHeap[ScatteringIntegrationPassCB.lightInjectionTargetIdx];
	RWTexture3D<float4> outputTexture = ResourceDescriptorHeap[ScatteringIntegrationPassCB.outputIdx];

	float3 accumulatedScattering = 0.0f;
	float  accumulatedTransmittance = 1.0f;
	float3 prevWorldPosition = FrameCB.cameraPosition.xyz;
	
	for (int z = 0; z < VOXEL_GRID_SIZE_Z; ++z)
	{
		uint3  voxelGridCoords = uint3(input.DispatchThreadId.xy, z);
		float3 worldPosition = GetWorldPositionFromVoxelCoords(voxelGridCoords, 0.0f, ScatteringIntegrationPassCB.voxelGridDimensions);
		float  distance = length(worldPosition - prevWorldPosition);

		float4 scatteringAndDensity = lightInjectionTarget[int3(input.DispatchThreadId.xy, z - 1)];
		const float3 scattering = scatteringAndDensity.rgb;
		const float density = scatteringAndDensity.a;
		float transmittance = exp(-density * distance);

		float3 scatteringOverSlice = scattering * (1 - transmittance) / max(density, 1e-6f);
		accumulatedScattering += scatteringOverSlice * accumulatedTransmittance;
		accumulatedTransmittance *= transmittance;

		outputTexture[voxelGridCoords] = float4(accumulatedScattering, accumulatedTransmittance);
		prevWorldPosition = worldPosition;
	}
}



struct VSToPS
{
	float4 Pos  : SV_POSITION;
	float2 Tex  : TEX;
};

struct CombineFogConstants
{
	uint fogIdx;
	uint depthIdx;
};
ConstantBuffer<CombineFogConstants> CombineFogPassCB : register(b1);

float4 CombineFogPS(VSToPS input) : SV_Target0
{
	float2 uv = input.Tex;
    Texture2D<float> depthTexture = ResourceDescriptorHeap[CombineFogPassCB.depthIdx];
	float depth = depthTexture.Sample(LinearBorderSampler, uv);
    float3 viewPosition = GetViewPosition(uv, depth);

	Texture3D<float4> fogTexture = ResourceDescriptorHeap[CombineFogPassCB.fogIdx];
	float fogZ = (viewPosition.z - FrameCB.cameraFar) / (FrameCB.cameraNear - FrameCB.cameraFar);
	fogZ = sqrt(fogZ);
	float4 scatteringTransmittance = fogTexture.SampleLevel(LinearClampSampler, float3(uv, fogZ), 0);
	return scatteringTransmittance;
}


