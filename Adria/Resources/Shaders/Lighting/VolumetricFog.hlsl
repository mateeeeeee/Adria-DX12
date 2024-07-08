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
	uint   voxelGridIdx;
	uint   voxelGridHistoryIdx;
    uint   blueNoiseIdx;
};
ConstantBuffer<LightInjectionConstants> PassCB : register(b1);


float SampleBlueNoise(uint3 coords)
{
	int2 noiseCoords = (coords.xy + int2(0, 1) * coords.z * BLUE_NOISE_TEXTURE_SIZE) % BLUE_NOISE_TEXTURE_SIZE;
	Texture2D<float> blueNoiseTexture = ResourceDescriptorHeap[PassCB.blueNoiseIdx];
    return blueNoiseTexture.Load(int3(noiseCoords, 0));
}

float3 GetUVWFromVoxelCoords(uint3 voxelCoords, float jitter)
{
	float2 texelUV = ((float2)voxelCoords.xy + 0.5f) / PassCB.voxelGridDimensions.xy;
	float n = FrameCB.cameraNear;
	float f = FrameCB.cameraFar;
    float viewZ = n * pow(f / n, (float(voxelCoords.z) + 0.5f + jitter) / float(VOXEL_GRID_SIZE_Z)); //maybe switch n and f?
	return float3(texelUV, viewZ / f);
}

float3 GetWorldPositionFromVoxelCoords(uint3 voxelCoords, float jitter)
{
	float3 uvw = GetUVWFromVoxelCoords(voxelCoords, jitter);
	return GetWorldPosition(uvw.xy, uvw.z);
}

[numthreads(8, 8, 8)]
void LightInjectionCS(CSInput input)
{
	uint3 voxelGridCoords = input.DispatchThreadId;
	if(any(voxelGridCoords >= PassCB.voxelGridDimensions)) return;

	const float jitter	 = (SampleBlueNoise(voxelGridCoords) - 0.5f);
    float3 worldPosition = GetWorldPositionFromVoxelCoords(voxelGridCoords, jitter);

	float3 cellAbsorption = 0.0f;
	float  cellDensity	  = 0.0f;

	StructuredBuffer<FogVolume> fogVolumeBuffer = ResourceDescriptorHeap[PassCB.fogVolumeBufferIdx];
	for(uint i = 0; i < PassCB.fogVolumesCount; ++i)
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
	float dither = InterleavedGradientNoise(voxelGridCoords.xy);
	float3 viewDirection = normalize(FrameCB.cameraPosition.xyz - worldPosition);

	if(any(inScattering > 0.0f))
	{
		StructuredBuffer<Light> lights	= ResourceDescriptorHeap[FrameCB.lightsIdx];
		uint lightCount, unused;
		lights.GetDimensions(lightCount, unused);
		for (uint i = 0; i < lightCount; ++i)
		{
			Light light = lights[i];
			if (!light.active || !light.volumetric) continue;

			//do the lighting stuff
		}
	}

	float4 newScattering = float4(inScattering * totalLighting, cellDensity);

#ifdef TEMPORAL_REPROJECTION
	float3 reprojUVW = 0.0f;
	Texture3D voxelGridTextureHistory = ResourceDescriptorHeap[PassCB.voxelGridHistoryIdx];
	float4 oldScattering = voxelGridTextureHistory[voxelGridCoords];
	float lerpFactor = 0.0f;
	newScattering = lerp(oldScattering, newScattering, lerpFactor);
	//todo finish this
#endif
	
	RWTexture3D<float4> voxelGridTexture = ResourceDescriptorHeap[PassCB.voxelGridIdx];
	voxelGridTexture[voxelGridCoords] = newScattering;
}

struct ScatteringAccumulationConstants
{
	uint voxelGridIdx;
	uint voxelGridHistoryIdx;
	uint fogVolumeBufferIdx;
};
ConstantBuffer<ScatteringAccumulationConstants> PassCB2 : register(b1);

[numthreads(8, 8, 8)]
void ScatteringAccumulationCS(CSInput input)
{
	
}




